#include "lupine/scripting/NodeRef.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/core/SignalObject.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/components/Timer.hpp"
#include "lupine/components/Tween.hpp"
#include "lupine/components/TweenSequence.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/asset/Assets.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/RpcManager.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace scripting {

namespace {

// A Windows x64 user-mode heap pointer always lies in (0x10000, 0x7FFF'FFFFFFFF].
// A NodeRef whose locked element pointer falls outside this range is corrupted
// (its weak_ptr storage was overwritten) — Lock() rejects it so callers no-op
// rather than dereference garbage and crash.
inline bool node_ptr_plausible(const core::Node* p) {
    const uintptr_t v = reinterpret_cast<uintptr_t>(p);
    return v >= 0x10000ull && v <= 0x00007FFFFFFFFFFFull;
}

// Build a transient ScriptAPI bound to a target node + scene tree so NodeRef can
// reuse the engine's existing per-node transform/query logic without duplicating
// it. The tree is the long-lived SceneManager stored on the handle, never a
// freeable per-component ScriptAPI.
void ConfigureApi(ScriptAPI& api, core::Node* node, core::SceneManager* tree) {
    api.SetOwner(node);
    if (tree) {
        api.SetSceneManager(tree);
    }
}

// Resolve the long-lived scene tree from a (possibly transient) component api.
// Handles capture the tree once at creation so they never dereference a freed
// ScriptAPI later (see the lifetime note in NodeRef.hpp).
inline core::SceneManager* TreeOf(ScriptAPI* api) {
    return api ? api->GetTree() : nullptr;
}

// Convert a JSON signal-argument payload (array, or a single value) into the
// engine's SignalArgs vector.
core::SignalArgs ToSignalArgs(const nlohmann::json& args) {
    core::SignalArgs out;
    if (args.is_array()) {
        for (const nlohmann::json& a : args) {
            out.push_back(a);
        }
    } else if (!args.is_null()) {
        out.push_back(args);
    }
    return out;
}

// Recursively remove UUIDs from serialized node JSON so a duplicated subtree
// receives fresh identifiers on deserialize.
void StripUUIDs(nlohmann::json& json) {
    if (json.contains("uuid")) {
        json.erase("uuid");
    }
    if (json.contains("components") && json["components"].is_array()) {
        for (auto& componentJson : json["components"]) {
            if (componentJson.contains("uuid")) {
                componentJson.erase("uuid");
            }
        }
    }
    if (json.contains("children") && json["children"].is_array()) {
        for (auto& childJson : json["children"]) {
            StripUUIDs(childJson);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// ScriptAPI* constructors: capture only the api's long-lived scene tree. A
// per-component ScriptAPI may be freed while a script still holds the handle
// (cached in a table, captured by a coroutine), so the handle must not store it.
// ---------------------------------------------------------------------------
ComponentRef::ComponentRef(std::weak_ptr<core::Component> component, ScriptAPI* api)
    : m_Component(component), m_Tree(TreeOf(api)) {}

NodeRef::NodeRef(std::weak_ptr<core::Node> node, ScriptAPI* api)
    : m_Node(node), m_Tree(TreeOf(api)) {}

TimerRef::TimerRef(std::weak_ptr<core::Component> timer, ScriptAPI* api)
    : m_Timer(timer), m_Tree(TreeOf(api)) {}

TweenRef::TweenRef(std::weak_ptr<core::Component> tween, ScriptAPI* api)
    : m_Tween(tween), m_Tree(TreeOf(api)) {}

SequenceRef::SequenceRef(std::weak_ptr<core::Component> sequence, ScriptAPI* api)
    : m_Sequence(sequence), m_Tree(TreeOf(api)) {}

SceneRef::SceneRef(core::Scene* scene, ScriptAPI* api)
    : m_Scene(scene), m_Tree(TreeOf(api)) {}

// ===========================================================================
// ComponentRef
// ===========================================================================

bool ComponentRef::IsValid() const {
    return static_cast<bool>(m_Component.lock());
}

std::string ComponentRef::GetTypeName() const {
    auto comp = m_Component.lock();
    return comp ? comp->GetTypeName() : std::string();
}

std::string ComponentRef::GetName() const {
    auto comp = m_Component.lock();
    return comp ? comp->GetName() : std::string();
}

bool ComponentRef::IsEnabled() const {
    auto comp = m_Component.lock();
    return comp ? comp->IsEnabled() : false;
}

void ComponentRef::SetEnabled(bool enabled) {
    auto comp = m_Component.lock();
    if (comp) {
        comp->SetEnabled(enabled);
    }
}

bool ComponentRef::HasProperty(const std::string& propName) const {
    auto comp = m_Component.lock();
    if (!comp) return false;
    return comp->GetPropertyRegistry().HasProperty(propName);
}

nlohmann::json ComponentRef::Get(const std::string& propName) const {
    auto comp = m_Component.lock();
    if (!comp) return nlohmann::json();

    const core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(propName);
    if (prop) {
        return prop->GetValueAsJson();
    }

    // Fall back to ISerializable-declared properties (read-only).
    nlohmann::json serialized = comp->Serialize();
    if (serialized.contains("properties") && serialized["properties"].contains(propName)) {
        return serialized["properties"][propName];
    }

    return nlohmann::json();
}

void ComponentRef::Set(const std::string& propName, const nlohmann::json& value) {
    auto comp = m_Component.lock();
    if (!comp) return;

    core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(propName);
    if (prop) {
        prop->SetValueFromJson(value);
        comp->OnPropertyChanged(propName, value);
    }
}

nlohmann::json ComponentRef::Call(const std::string& method, const nlohmann::json& args) const {
    auto comp = m_Component.lock();
    if (!comp) return nlohmann::json();
    return comp->CallMethod(method, args);
}

NodeRef ComponentRef::GetOwner() const {
    auto comp = m_Component.lock();
    if (!comp) return NodeRef();
    return NodeRef::FromRawTree(comp->GetOwner(), m_Tree);
}

void ComponentRef::EmitSignal(const std::string& signal, const nlohmann::json& args) const {
    auto comp = m_Component.lock();
    if (comp) {
        comp->Emit(signal, ToSignalArgs(args));
    }
}

uint64_t ComponentRef::ConnectSignal(const std::string& signal, const NodeRef& target,
                                     const std::string& method, uint32_t flags) const {
    auto comp = m_Component.lock();
    auto targetNode = target.Lock();
    if (!comp || !targetNode) {
        return 0;
    }
    return comp->Connect(signal, targetNode.get(), method, flags);
}

void ComponentRef::DisconnectSignal(const std::string& signal, uint64_t connectionId) const {
    auto comp = m_Component.lock();
    if (comp) {
        comp->Disconnect(signal, connectionId);
    }
}

void ComponentRef::DisconnectSignalMethod(const std::string& signal, const NodeRef& target,
                                          const std::string& method) const {
    auto comp = m_Component.lock();
    auto targetNode = target.Lock();
    if (comp && targetNode) {
        comp->DisconnectTarget(signal, targetNode.get(), method);
    }
}

bool ComponentRef::IsSignalConnected(const std::string& signal) const {
    auto comp = m_Component.lock();
    return comp ? comp->IsConnected(signal) : false;
}

void ComponentRef::AddUserSignal(const std::string& name) const {
    auto comp = m_Component.lock();
    if (comp) {
        comp->AddUserSignal(name);
    }
}

std::vector<std::string> ComponentRef::GetSignalList() const {
    std::vector<std::string> names;
    auto comp = m_Component.lock();
    if (comp) {
        for (const core::SignalDesc& desc : comp->GetSignalDescriptors()) {
            names.push_back(desc.name);
        }
    }
    return names;
}

SignalAwaiter ComponentRef::AwaitSignal(const std::string& signal) const {
    auto comp = m_Component.lock();
    if (!comp) return SignalAwaiter();
    return SignalAwaiter::Connect(comp.get(), signal);
}

// ===========================================================================
// NodeRef
// ===========================================================================

NodeRef NodeRef::FromRawTree(core::Node* node, core::SceneManager* tree) {
    if (!node || !node_ptr_plausible(node)) return NodeRef();
    try {
        return NodeRef(std::weak_ptr<core::Node>(node->shared_from_this()), tree);
    } catch (const std::bad_weak_ptr&) {
        return NodeRef();
    }
}

NodeRef NodeRef::FromSharedTree(const std::shared_ptr<core::Node>& node, core::SceneManager* tree) {
    return NodeRef(std::weak_ptr<core::Node>(node), tree);
}

NodeRef NodeRef::FromRaw(core::Node* node, ScriptAPI* api) {
    return FromRawTree(node, TreeOf(api));
}

NodeRef NodeRef::FromShared(const std::shared_ptr<core::Node>& node, ScriptAPI* api) {
    return FromSharedTree(node, TreeOf(api));
}

bool NodeRef::IsValid() const {
    return static_cast<bool>(Lock());
}

std::shared_ptr<core::Node> NodeRef::Lock() const {
    std::shared_ptr<core::Node> node = m_Node.lock();
    if (node && !node_ptr_plausible(node.get())) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            LOG_ERROR(LogCategory::Scripting,
                "[NodeRef] discarding corrupted node handle (element ptr {} non-canonical); "
                "treating handle as dead so operations no-op. This indicates heap corruption "
                "upstream — capture under PageHeap/ASan to locate the offending write.",
                reinterpret_cast<const void*>(node.get()));
        }
        return nullptr;
    }
    return node;
}

std::string NodeRef::GetName() const {
    auto node = Lock();
    return node ? node->GetName() : std::string();
}

void NodeRef::SetName(const std::string& name) {
    auto node = Lock();
    if (node) node->SetName(name);
}

std::string NodeRef::GetUUID() const {
    auto node = Lock();
    return node ? node->GetUUID().ToString() : std::string();
}

std::string NodeRef::GetPath() const {
    auto node = Lock();
    return node ? node->GetPath() : std::string();
}

std::string NodeRef::GetTypeName() const {
    auto node = Lock();
    return node ? node->GetTypeName() : std::string();
}

bool NodeRef::IsActive() const {
    auto node = Lock();
    return node ? node->IsActive() : false;
}

void NodeRef::SetActive(bool active) {
    auto node = Lock();
    if (node) node->SetActive(active);
}

bool NodeRef::IsVisible() const {
    auto node = Lock();
    return node ? node->IsVisible() : false;
}

void NodeRef::SetVisible(bool visible) {
    auto node = Lock();
    if (node) node->SetVisible(visible);
}

bool NodeRef::IsUniqueNameInOwner() const {
    auto node = Lock();
    return node ? node->IsUniqueNameInOwner() : false;
}

void NodeRef::SetUniqueNameInOwner(bool unique) {
    auto node = Lock();
    if (node) node->SetUniqueNameInOwner(unique);
}

void NodeRef::AddToGroup(const std::string& group) const {
    auto node = Lock();
    if (node) node->AddToGroup(group);
}

void NodeRef::RemoveFromGroup(const std::string& group) const {
    auto node = Lock();
    if (node) node->RemoveFromGroup(group);
}

bool NodeRef::IsInGroup(const std::string& group) const {
    auto node = Lock();
    return node ? node->IsInGroup(group) : false;
}

std::vector<std::string> NodeRef::GetGroups() const {
    auto node = Lock();
    return node ? node->GetGroups() : std::vector<std::string>();
}

bool NodeRef::ImplementsInterface(const std::string& interfaceName) const {
    auto node = Lock();
    return node ? node->ImplementsInterface(interfaceName) : false;
}

std::vector<std::string> NodeRef::GetInterfaces() const {
    auto node = Lock();
    return node ? node->GetImplementedInterfaces() : std::vector<std::string>();
}

nlohmann::json NodeRef::VerifyInterface(const std::string& interfaceName) const {
    auto node = Lock();
    if (!node) {
        return nlohmann::json();
    }
    return node->VerifyInterface(interfaceName);
}

NodeRef NodeRef::GetParent() const {
    auto node = Lock();
    if (!node) return NodeRef();
    return FromSharedTree(node->GetParentShared(), m_Tree);
}

NodeRef NodeRef::GetChild(const std::string& name) const {
    auto node = Lock();
    if (!node) return NodeRef();
    return FromSharedTree(node->GetChild(name), m_Tree);
}

NodeRef NodeRef::GetChildAt(int index) const {
    auto node = Lock();
    if (!node || index < 0) return NodeRef();
    return FromSharedTree(node->GetChild(static_cast<size_t>(index)), m_Tree);
}

int NodeRef::GetChildCount() const {
    auto node = Lock();
    return node ? static_cast<int>(node->GetChildCount()) : 0;
}

std::vector<NodeRef> NodeRef::GetChildren() const {
    std::vector<NodeRef> result;
    auto node = Lock();
    if (!node) return result;
    for (const auto& child : node->GetChildren()) {
        result.push_back(FromSharedTree(child, m_Tree));
    }
    return result;
}

NodeRef NodeRef::FindNode(const std::string& path) const {
    auto node = Lock();
    if (!node || path.empty()) return NodeRef();

    // Unique-name access scoped to this node's owner scope.
    if (path[0] == '%') {
        size_t slash = path.find('/');
        std::string uniqueName = (slash == std::string::npos)
            ? path.substr(1)
            : path.substr(1, slash - 1);

        core::Node* resolved = node->ResolveUniqueName(uniqueName);
        if (!resolved) return NodeRef();

        if (slash == std::string::npos) {
            return FromRawTree(resolved, m_Tree);
        }
        std::string remainder = path.substr(slash + 1);
        if (remainder.empty()) {
            return FromRawTree(resolved, m_Tree);
        }
        return FromSharedTree(resolved->FindNode(remainder), m_Tree);
    }

    return FromSharedTree(node->FindNode(path), m_Tree);
}

bool NodeRef::HasNode(const std::string& path) const {
    return FindNode(path).IsValid();
}

void NodeRef::AddChild(const NodeRef& child) {
    auto node = Lock();
    auto childNode = child.Lock();
    if (node && childNode) {
        node->AddChild(childNode);
    }
}

void NodeRef::RemoveChild(const NodeRef& child) {
    auto node = Lock();
    auto childNode = child.Lock();
    if (node && childNode) {
        node->RemoveChild(childNode);
    }
}

void NodeRef::ReparentTo(const NodeRef& newParent) {
    auto node = Lock();
    auto parentNode = newParent.Lock();
    if (node && parentNode) {
        node->ReparentTo(parentNode.get());
    }
}

int NodeRef::GetSiblingIndex() const {
    auto node = Lock();
    return node ? node->GetIndexInParent() : -1;
}

void NodeRef::SetSiblingIndex(int index) {
    auto node = Lock();
    if (node && index >= 0) {
        node->SetSiblingIndex(static_cast<size_t>(index));
    }
}

int NodeRef::GetChildIndex(const NodeRef& child) const {
    auto node = Lock();
    auto childNode = child.Lock();
    if (!node || !childNode) return -1;
    return node->GetChildIndex(childNode.get());
}

void NodeRef::MoveChild(const NodeRef& child, int index) {
    auto node = Lock();
    auto childNode = child.Lock();
    if (node && childNode && index >= 0) {
        node->MoveChild(childNode, static_cast<size_t>(index));
    }
}

void NodeRef::QueueFree() {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api;
    ConfigureApi(api, node.get(), m_Tree);
    api.QueueFree(node.get());
}

void NodeRef::QueueFreeDeferred() {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api;
    ConfigureApi(api, node.get(), m_Tree);
    api.QueueFreeDeferred(node.get());
}

void NodeRef::Free() {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api;
    ConfigureApi(api, node.get(), m_Tree);
    api.Free(node.get());
}

NodeRef NodeRef::Duplicate() const {
    auto node = Lock();
    if (!node) return NodeRef();

    nlohmann::json data = node->Serialize();
    StripUUIDs(data);

    if (!data.contains("type")) return NodeRef();
    std::string typeName = data["type"].get<std::string>();

    auto clone = std::dynamic_pointer_cast<core::Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));
    if (!clone) return NodeRef();

    clone->Deserialize(data);

    core::Node* parent = node->GetParent();
    if (parent) {
        parent->AddChild(clone);
    }

    return FromSharedTree(clone, m_Tree);
}

// --- Transform 2D ----------------------------------------------------------

math::Vec2 NodeRef::GetPosition2D() const {
    auto node = Lock();
    if (!node) return math::Vec2(0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetPosition2D();
}

void NodeRef::SetPosition2D(float x, float y) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetPosition2D(x, y);
}

void NodeRef::Translate2D(float dx, float dy) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.Translate2D(dx, dy);
}

float NodeRef::GetRotation2D() const {
    auto node = Lock();
    if (!node) return 0.0f;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetRotation2D();
}

void NodeRef::SetRotation2D(float degrees) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetRotation2D(degrees);
}

math::Vec2 NodeRef::GetScale2D() const {
    auto node = Lock();
    if (!node) return math::Vec2(1.0f, 1.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetScale2D();
}

void NodeRef::SetScale2D(float x, float y) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetScale2D(x, y);
}

math::Vec2 NodeRef::GetGlobalPosition2D() const {
    auto node = Lock();
    if (!node) return math::Vec2(0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalPosition2D();
}

void NodeRef::SetGlobalPosition2D(float x, float y) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetGlobalPosition2D(x, y);
}

float NodeRef::GetGlobalRotation2D() const {
    auto node = Lock();
    if (!node) return 0.0f;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalRotation2D();
}

void NodeRef::SetGlobalRotation2D(float degrees) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetGlobalRotation2D(degrees);
}

math::Vec2 NodeRef::GetGlobalScale2D() const {
    auto node = Lock();
    if (!node) return math::Vec2(1.0f, 1.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalScale2D();
}

// --- Transform 3D ----------------------------------------------------------

math::Vec3 NodeRef::GetPosition3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(0.0f, 0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetPosition3D();
}

void NodeRef::SetPosition3D(float x, float y, float z) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetPosition3D(x, y, z);
}

void NodeRef::Translate3D(float dx, float dy, float dz) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.Translate3D(dx, dy, dz);
}

math::Vec3 NodeRef::GetRotation3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(0.0f, 0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetRotation3D();
}

void NodeRef::SetRotation3D(float pitch, float yaw, float roll) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetRotation3D(pitch, yaw, roll);
}

math::Vec3 NodeRef::GetScale3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(1.0f, 1.0f, 1.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetScale3D();
}

void NodeRef::SetScale3D(float x, float y, float z) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetScale3D(x, y, z);
}

math::Vec3 NodeRef::GetGlobalPosition3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(0.0f, 0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalPosition3D();
}

void NodeRef::SetGlobalPosition3D(float x, float y, float z) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetGlobalPosition3D(x, y, z);
}

math::Vec3 NodeRef::GetGlobalRotation3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(0.0f, 0.0f, 0.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalRotation3D();
}

void NodeRef::SetGlobalRotation3D(float pitch, float yaw, float roll) {
    auto node = Lock();
    if (!node) return;
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    api.SetGlobalRotation3D(pitch, yaw, roll);
}

math::Vec3 NodeRef::GetGlobalScale3D() const {
    auto node = Lock();
    if (!node) return math::Vec3(1.0f, 1.0f, 1.0f);
    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    return api.GetGlobalScale3D();
}

float NodeRef::DistanceTo(const NodeRef& other) const {
    auto node = Lock();
    auto otherNode = other.Lock();
    if (!node || !otherNode) return 0.0f;

    ScriptAPI api; ConfigureApi(api, node.get(), m_Tree);
    if (dynamic_cast<core::Node3D*>(node.get())) {
        return api.DistanceTo3D(otherNode.get());
    }
    return api.DistanceTo2D(otherNode.get());
}

// --- Components ------------------------------------------------------------

ComponentRef NodeRef::GetComponent(const std::string& typeName) const {
    auto node = Lock();
    if (!node) return ComponentRef();
    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->GetTypeName() == typeName) {
            return ComponentRef(std::weak_ptr<core::Component>(comp), m_Tree);
        }
    }
    return ComponentRef();
}

std::vector<ComponentRef> NodeRef::GetComponents(const std::string& typeName) const {
    std::vector<ComponentRef> result;
    auto node = Lock();
    if (!node) return result;
    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->GetTypeName() == typeName) {
            result.push_back(ComponentRef(std::weak_ptr<core::Component>(comp), m_Tree));
        }
    }
    return result;
}

bool NodeRef::HasComponent(const std::string& typeName) const {
    return GetComponent(typeName).IsValid();
}

ComponentRef NodeRef::AddComponent(const std::string& typeName) {
    auto node = Lock();
    if (!node) return ComponentRef();

    auto comp = std::dynamic_pointer_cast<core::Component>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));
    if (!comp) return ComponentRef();

    comp->RegisterProperties();
    node->AddComponent(comp);
    return ComponentRef(std::weak_ptr<core::Component>(comp), m_Tree);
}

void NodeRef::RemoveComponent(const ComponentRef& component) {
    auto node = Lock();
    auto comp = component.Lock();
    if (node && comp) {
        node->RemoveComponent(comp);
    }
}

TweenRef NodeRef::CreateTween(const std::string& channel, const nlohmann::json& toValue,
                             float duration, const std::string& easing) const {
    auto node = Lock();
    if (!node) return TweenRef();

    auto tween = std::make_shared<components::Tween>();
    tween->RegisterProperties();
    tween->Configure(channel, toValue, duration, easing);
    tween->SetAutoRemove(true);

    node->AddComponent(tween);
    tween->Start();
    return TweenRef(std::weak_ptr<core::Component>(tween), m_Tree);
}

SequenceRef NodeRef::CreateSequence() const {
    auto node = Lock();
    if (!node) return SequenceRef();

    auto seq = std::make_shared<components::TweenSequence>();
    seq->RegisterProperties();
    node->AddComponent(seq);
    return SequenceRef(std::weak_ptr<core::Component>(seq), m_Tree);
}

// --- Property bag ----------------------------------------------------------

bool NodeRef::HasProperty(const std::string& propName) const {
    auto node = Lock();
    if (!node) return false;

    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->GetPropertyRegistry().HasProperty(propName)) {
            return true;
        }
    }

    node->RegisterProperties();
    nlohmann::json serialized = node->Serialize();
    return serialized.contains("properties") && serialized["properties"].contains(propName);
}

nlohmann::json NodeRef::Get(const std::string& propName) const {
    auto node = Lock();
    if (!node) return nlohmann::json();

    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->GetPropertyRegistry().HasProperty(propName)) {
            const core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(propName);
            if (prop) {
                return prop->GetValueAsJson();
            }
        }
    }

    node->RegisterProperties();
    nlohmann::json serialized = node->Serialize();
    if (serialized.contains("properties") && serialized["properties"].contains(propName)) {
        return serialized["properties"][propName];
    }

    return nlohmann::json();
}

void NodeRef::Set(const std::string& propName, const nlohmann::json& value) {
    auto node = Lock();
    if (!node) return;

    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->GetPropertyRegistry().HasProperty(propName)) {
            core::ComponentProperty* prop = comp->GetPropertyRegistry().GetProperty(propName);
            if (prop) {
                prop->SetValueFromJson(value);
                comp->OnPropertyChanged(propName, value);
                return;
            }
        }
    }

    // Node-level property: deserialize a single property without touching the
    // node's components/children (Node::Deserialize guards each section).
    node->RegisterProperties();
    nlohmann::json patch;
    patch["properties"][propName] = value;
    node->Deserialize(patch);
}

bool NodeRef::HasMethod(const std::string& method) const {
    auto node = Lock();
    if (!node) return false;

    for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
        if (comp && comp->HasInterfaceMethod(method)) {
            return true;
        }
    }
    return false;
}

nlohmann::json NodeRef::Call(const std::string& method, const nlohmann::json& args) const {
    auto node = Lock();
    if (!node) return nlohmann::json();

    // Dispatch to the one component that owns the method rather than to every
    // component: a node may carry several scripts, and only the one defining
    // `method` should run.
    for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
        if (comp && comp->HasInterfaceMethod(method)) {
            return comp->CallMethod(method, args);
        }
    }
    return nlohmann::json();
}

void NodeRef::EmitSignal(const std::string& signal, const nlohmann::json& args) const {
    auto node = Lock();
    if (node) {
        node->Emit(signal, ToSignalArgs(args));
    }
}

namespace {

// JSON args for RPCs cross as an array, matching the signal convention.
nlohmann::json ToRpcArgs(const nlohmann::json& args) {
    if (args.is_array()) {
        return args;
    }
    nlohmann::json array = nlohmann::json::array();
    if (!args.is_null()) {
        array.push_back(args);
    }
    return array;
}

components::NetworkObject* NetObjectOf(const std::shared_ptr<core::Node>& node) {
    if (!node) {
        return nullptr;
    }
    std::shared_ptr<core::Component> comp = node->GetComponent("NetworkObject");
    return dynamic_cast<components::NetworkObject*>(comp.get());
}

} // namespace

void NodeRef::Rpc(const std::string& method, const nlohmann::json& args) const {
    auto node = Lock();
    if (!node) {
        return;
    }
    network::NetworkManager::GetInstance().Rpc().SendNodeRpc(node.get(), method, ToRpcArgs(args));
}

void NodeRef::RpcId(uint32_t peerId, const std::string& method, const nlohmann::json& args) const {
    auto node = Lock();
    if (!node) {
        return;
    }
    network::NetworkManager::GetInstance().Rpc().SendNodeRpc(
        node.get(), method, ToRpcArgs(args), static_cast<network::PeerId>(peerId));
}

void NodeRef::RpcUnreliable(const std::string& method, const nlohmann::json& args) const {
    auto node = Lock();
    if (!node) {
        return;
    }
    network::NetworkManager::GetInstance().Rpc().SendNodeRpc(
        node.get(), method, ToRpcArgs(args), network::kInvalidPeerId,
        /*overrideTransfer*/ true, network::TransferMode::Unreliable);
}

void NodeRef::SetMultiplayerAuthority(uint32_t peerId) const {
    auto node = Lock();
    components::NetworkObject* netObject = NetObjectOf(node);
    if (netObject != nullptr) {
        netObject->SetOwnerPeerId(static_cast<network::PeerId>(peerId));
    }
}

uint32_t NodeRef::GetMultiplayerAuthority() const {
    auto node = Lock();
    components::NetworkObject* netObject = NetObjectOf(node);
    return netObject != nullptr ? netObject->GetOwnerPeerId() : network::kServerPeerId;
}

bool NodeRef::IsMultiplayerAuthority() const {
    auto node = Lock();
    components::NetworkObject* netObject = NetObjectOf(node);
    if (netObject != nullptr) {
        return netObject->IsAuthority();
    }
    // No NetworkObject: in single-player every node is locally authoritative; in a
    // session, only the server side is considered authoritative for plain nodes.
    network::NetworkManager& net = network::NetworkManager::GetInstance();
    return !net.IsActive() || net.IsServer();
}

uint32_t NodeRef::GetNetworkId() const {
    auto node = Lock();
    components::NetworkObject* netObject = NetObjectOf(node);
    return netObject != nullptr ? netObject->GetNetworkId() : network::kInvalidNetworkId;
}

uint64_t NodeRef::ConnectSignal(const std::string& signal, const NodeRef& target,
                                const std::string& method, uint32_t flags) const {
    auto node = Lock();
    auto targetNode = target.Lock();
    if (!node || !targetNode) {
        return 0;
    }
    return node->Connect(signal, targetNode.get(), method, flags);
}

void NodeRef::DisconnectSignal(const std::string& signal, uint64_t connectionId) const {
    auto node = Lock();
    if (node) {
        node->Disconnect(signal, connectionId);
    }
}

void NodeRef::DisconnectSignalMethod(const std::string& signal, const NodeRef& target,
                                     const std::string& method) const {
    auto node = Lock();
    auto targetNode = target.Lock();
    if (node && targetNode) {
        node->DisconnectTarget(signal, targetNode.get(), method);
    }
}

bool NodeRef::IsSignalConnected(const std::string& signal) const {
    auto node = Lock();
    return node ? node->IsConnected(signal) : false;
}

void NodeRef::AddUserSignal(const std::string& name) const {
    auto node = Lock();
    if (node) {
        node->AddUserSignal(name);
    }
}

std::vector<std::string> NodeRef::GetSignalList() const {
    std::vector<std::string> names;
    auto node = Lock();
    if (node) {
        for (const core::SignalDesc& desc : node->GetSignalDescriptors()) {
            names.push_back(desc.name);
        }
    }
    return names;
}

SignalAwaiter NodeRef::AwaitSignal(const std::string& signal) const {
    auto node = Lock();
    if (!node) return SignalAwaiter();
    return SignalAwaiter::Connect(node.get(), signal);
}

// ===========================================================================
// SignalAwaiter
// ===========================================================================

SignalAwaiter SignalAwaiter::Connect(core::SignalObject* source, const std::string& signal) {
    SignalAwaiter awaiter;
    if (!source) {
        return awaiter;
    }
    awaiter.m_Fired = std::make_shared<bool>(false);
    awaiter.m_SourceLifetime = source->GetSignalLifetime();
    awaiter.m_Signal = signal;

    std::shared_ptr<bool> fired = awaiter.m_Fired;
    awaiter.m_ConnectionId = source->Connect(signal,
        [fired](const core::SignalArgs&) { *fired = true; },
        core::Connect_OneShot);
    return awaiter;
}

bool SignalAwaiter::IsSourceAlive() const {
    if (!m_Fired) {
        return false;
    }
    std::shared_ptr<core::SignalLifetime> lifetime = m_SourceLifetime.lock();
    return lifetime && lifetime->obj != nullptr;
}

void SignalAwaiter::Cancel() const {
    if (m_ConnectionId == 0) {
        return;
    }
    if (auto lifetime = m_SourceLifetime.lock()) {
        if (lifetime->obj) {
            lifetime->obj->Disconnect(m_Signal, m_ConnectionId);
        }
    }
}

// ===========================================================================
// TimerRef
// ===========================================================================

namespace {

// Lock the wrapped component and downcast to a Timer in one step.
components::Timer* LockTimer(const std::weak_ptr<core::Component>& weak,
                            std::shared_ptr<core::Component>& outShared) {
    outShared = weak.lock();
    return dynamic_cast<components::Timer*>(outShared.get());
}

} // namespace

TimerRef TimerRef::FromComponent(core::Component* timer, ScriptAPI* api) {
    if (!timer) return TimerRef();
    core::Node* owner = timer->GetOwner();
    if (!owner) return TimerRef();
    for (const auto& comp : owner->GetComponents()) {
        if (comp.get() == timer) {
            return TimerRef(std::weak_ptr<core::Component>(comp), api);
        }
    }
    return TimerRef();
}

bool TimerRef::IsValid() const {
    std::shared_ptr<core::Component> shared;
    return LockTimer(m_Timer, shared) != nullptr;
}

std::string TimerRef::GetName() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetName() : std::string();
}

void TimerRef::Start() const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->Start();
}

void TimerRef::Stop() const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->Stop();
}

void TimerRef::Reset() const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->Reset();
}

void TimerRef::Restart() const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->Restart();
}

void TimerRef::Remove() const {
    std::shared_ptr<core::Component> shared = m_Timer.lock();
    if (!shared) return;
    core::Node* owner = shared->GetOwner();
    if (owner) {
        owner->RemoveComponent(shared);
    }
}

bool TimerRef::IsRunning() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->IsRunning() : false;
}

bool TimerRef::IsFinished() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->IsFinished() : false;
}

float TimerRef::GetTimeLeft() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetTimeRemaining() : 0.0f;
}

int TimerRef::GetFireCount() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetFireCount() : 0;
}

float TimerRef::GetDuration() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetDuration() : 0.0f;
}

void TimerRef::SetDuration(float duration) const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->SetDuration(duration);
}

float TimerRef::GetElapsed() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetElapsed() : 0.0f;
}

void TimerRef::SetElapsed(float elapsed) const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->SetElapsed(elapsed);
}

bool TimerRef::GetLoop() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetLoop() : false;
}

void TimerRef::SetLoop(bool loop) const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->SetLoop(loop);
}

int TimerRef::GetRepeatCount() const {
    std::shared_ptr<core::Component> shared;
    components::Timer* timer = LockTimer(m_Timer, shared);
    return timer ? timer->GetRepeatCount() : -1;
}

void TimerRef::SetRepeatCount(int repeatCount) const {
    std::shared_ptr<core::Component> shared;
    if (components::Timer* timer = LockTimer(m_Timer, shared)) timer->SetRepeatCount(repeatCount);
}

NodeRef TimerRef::GetOwner() const {
    std::shared_ptr<core::Component> shared = m_Timer.lock();
    if (!shared) return NodeRef();
    return NodeRef::FromRawTree(shared->GetOwner(), m_Tree);
}

ComponentRef TimerRef::AsComponent() const {
    return ComponentRef(m_Timer, m_Tree);
}

// ===========================================================================
// TweenRef
// ===========================================================================

namespace {

components::Tween* LockTween(const std::weak_ptr<core::Component>& weak,
                            std::shared_ptr<core::Component>& outShared) {
    outShared = weak.lock();
    return dynamic_cast<components::Tween*>(outShared.get());
}

} // namespace

TweenRef TweenRef::FromComponent(core::Component* tween, ScriptAPI* api) {
    if (!tween) return TweenRef();
    core::Node* owner = tween->GetOwner();
    if (!owner) return TweenRef();
    for (const auto& comp : owner->GetComponents()) {
        if (comp.get() == tween) {
            return TweenRef(std::weak_ptr<core::Component>(comp), api);
        }
    }
    return TweenRef();
}

bool TweenRef::IsValid() const {
    std::shared_ptr<core::Component> shared;
    return LockTween(m_Tween, shared) != nullptr;
}

std::string TweenRef::GetName() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->GetName() : std::string();
}

void TweenRef::Play() const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) {
        if (tween->IsFinished()) tween->Restart(); else tween->SetRunning(true);
    }
}

void TweenRef::Pause() const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->Stop();
}

void TweenRef::Stop() const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) {
        tween->Stop();
        tween->Reset();
    }
}

void TweenRef::Restart() const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->Restart();
}

void TweenRef::Kill() const {
    std::shared_ptr<core::Component> shared = m_Tween.lock();
    if (!shared) return;
    core::Node* owner = shared->GetOwner();
    if (owner) owner->RemoveComponent(shared);
}

bool TweenRef::IsRunning() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->IsRunning() : false;
}

bool TweenRef::IsFinished() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->IsFinished() : false;
}

float TweenRef::GetProgress() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->GetProgress() : 0.0f;
}

float TweenRef::GetDuration() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->GetDuration() : 0.0f;
}

void TweenRef::SetDuration(float duration) const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->SetDuration(duration);
}

std::string TweenRef::GetEasing() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->GetEasing() : std::string();
}

void TweenRef::SetEasing(const std::string& easing) const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->SetEasing(easing);
}

bool TweenRef::GetLoop() const {
    std::shared_ptr<core::Component> shared;
    components::Tween* tween = LockTween(m_Tween, shared);
    return tween ? tween->GetLoop() : false;
}

void TweenRef::SetLoop(bool loop) const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->SetLoop(loop);
}

void TweenRef::SetAutoRemove(bool autoRemove) const {
    std::shared_ptr<core::Component> shared;
    if (components::Tween* tween = LockTween(m_Tween, shared)) tween->SetAutoRemove(autoRemove);
}

NodeRef TweenRef::GetOwner() const {
    std::shared_ptr<core::Component> shared = m_Tween.lock();
    if (!shared) return NodeRef();
    return NodeRef::FromRawTree(shared->GetOwner(), m_Tree);
}

ComponentRef TweenRef::AsComponent() const {
    return ComponentRef(m_Tween, m_Tree);
}

// ===========================================================================
// SequenceRef
// ===========================================================================

namespace {

components::TweenSequence* LockSequence(const std::weak_ptr<core::Component>& weak,
                                        std::shared_ptr<core::Component>& outShared) {
    outShared = weak.lock();
    return dynamic_cast<components::TweenSequence*>(outShared.get());
}

} // namespace

SequenceRef SequenceRef::FromComponent(core::Component* sequence, ScriptAPI* api) {
    if (!sequence) return SequenceRef();
    core::Node* owner = sequence->GetOwner();
    if (!owner) return SequenceRef();
    for (const auto& comp : owner->GetComponents()) {
        if (comp.get() == sequence) {
            return SequenceRef(std::weak_ptr<core::Component>(comp), api);
        }
    }
    return SequenceRef();
}

bool SequenceRef::IsValid() const {
    std::shared_ptr<core::Component> shared;
    return LockSequence(m_Sequence, shared) != nullptr;
}

std::string SequenceRef::GetName() const {
    std::shared_ptr<core::Component> shared;
    components::TweenSequence* seq = LockSequence(m_Sequence, shared);
    return seq ? seq->GetName() : std::string();
}

SequenceRef SequenceRef::Append(const std::string& channel, const nlohmann::json& toValue,
                                float duration, const std::string& easing, bool parallel) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->AppendTween(nullptr, channel, toValue, duration, easing, parallel);
    }
    return *this;
}

SequenceRef SequenceRef::AppendOn(const NodeRef& target, const std::string& channel,
                                  const nlohmann::json& toValue, float duration,
                                  const std::string& easing, bool parallel) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->AppendTween(target.Lock().get(), channel, toValue, duration, easing, parallel);
    }
    return *this;
}

SequenceRef SequenceRef::AppendInterval(float duration, bool parallel) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->AppendInterval(duration, parallel);
    }
    return *this;
}

SequenceRef SequenceRef::AppendCallback(const std::string& method, bool parallel) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->AppendCallback(nullptr, method, parallel);
    }
    return *this;
}

SequenceRef SequenceRef::AppendCallbackOn(const NodeRef& target, const std::string& method,
                                          bool parallel) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->AppendCallback(target.Lock().get(), method, parallel);
    }
    return *this;
}

SequenceRef SequenceRef::Play() const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) {
        seq->Play();
    }
    return *this;
}

void SequenceRef::Stop() const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->Stop();
}

void SequenceRef::Reset() const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->Reset();
}

void SequenceRef::Restart() const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->Restart();
}

void SequenceRef::Kill() const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->Kill();
}

bool SequenceRef::IsRunning() const {
    std::shared_ptr<core::Component> shared;
    components::TweenSequence* seq = LockSequence(m_Sequence, shared);
    return seq ? seq->IsRunning() : false;
}

bool SequenceRef::IsFinished() const {
    std::shared_ptr<core::Component> shared;
    components::TweenSequence* seq = LockSequence(m_Sequence, shared);
    return seq ? seq->IsFinished() : false;
}

SequenceRef SequenceRef::SetLoops(int loops) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->SetLoops(loops);
    return *this;
}

int SequenceRef::GetLoops() const {
    std::shared_ptr<core::Component> shared;
    components::TweenSequence* seq = LockSequence(m_Sequence, shared);
    return seq ? seq->GetLoops() : 0;
}

SequenceRef SequenceRef::SetAutoRemove(bool autoRemove) const {
    std::shared_ptr<core::Component> shared;
    if (components::TweenSequence* seq = LockSequence(m_Sequence, shared)) seq->SetAutoRemove(autoRemove);
    return *this;
}

int SequenceRef::GetStepCount() const {
    std::shared_ptr<core::Component> shared;
    components::TweenSequence* seq = LockSequence(m_Sequence, shared);
    return seq ? static_cast<int>(seq->GetStepCount()) : 0;
}

NodeRef SequenceRef::GetOwner() const {
    std::shared_ptr<core::Component> shared = m_Sequence.lock();
    if (!shared) return NodeRef();
    return NodeRef::FromRawTree(shared->GetOwner(), m_Tree);
}

ComponentRef SequenceRef::AsComponent() const {
    return ComponentRef(m_Sequence, m_Tree);
}

// ===========================================================================
// SceneRef
// ===========================================================================

bool SceneRef::IsLiveScene(core::SceneManager* tree, core::Scene* scene) {
    if (!tree || !scene) {
        return false;
    }
    if (tree->GetCurrentScene() == scene || tree->GetRootScene() == scene) {
        return true;
    }
    for (const std::shared_ptr<core::Scene>& autoload : tree->GetAutoloadScenes()) {
        if (autoload.get() == scene) {
            return true;
        }
    }
    return false;
}

std::string SceneRef::GetName() const {
    return m_Scene ? m_Scene->GetName() : std::string();
}

std::string SceneRef::GetPath() const {
    return m_Scene ? m_Scene->GetFilePath() : std::string();
}

NodeRef SceneRef::GetRoot() const {
    if (!m_Scene) return NodeRef();
    return NodeRef::FromSharedTree(m_Scene->GetRoot(), m_Tree);
}

NodeRef SceneRef::FindNode(const std::string& path) const {
    if (!m_Scene) return NodeRef();
    return NodeRef::FromSharedTree(m_Scene->FindNode(path), m_Tree);
}

NodeRef SceneRef::FindNodeByUUID(const std::string& uuid) const {
    if (!m_Scene) return NodeRef();
    return NodeRef::FromSharedTree(m_Scene->FindNodeByUUID(core::UUID::FromString(uuid)), m_Tree);
}

// ===========================================================================
// TreeRef
// ===========================================================================

NodeRef TreeRef::GetRoot() const {
    if (!m_Manager) return NodeRef();
    core::Scene* scene = m_Manager->GetCurrentScene();
    if (!scene) return NodeRef();
    return NodeRef::FromSharedTree(scene->GetRoot(), m_Manager);
}

SceneRef TreeRef::GetCurrentScene() const {
    if (!m_Manager) return SceneRef();
    return SceneRef(m_Manager->GetCurrentScene(), m_Manager);
}

std::string TreeRef::GetCurrentScenePath() const {
    if (!m_Manager) return std::string();
    core::Scene* scene = m_Manager->GetCurrentScene();
    return scene ? scene->GetFilePath() : std::string();
}

void TreeRef::ChangeScene(const std::string& scenePath) const {
    if (m_Manager) m_Manager->SwitchScene(scenePath);
}

void TreeRef::ReloadScene() const {
    if (!m_Manager) return;
    core::Scene* scene = m_Manager->GetCurrentScene();
    if (scene && !scene->GetFilePath().empty()) {
        m_Manager->SwitchScene(scene->GetFilePath());
    }
}

void TreeRef::AddScene(const std::string& scenePath) const {
    if (m_Manager) m_Manager->AddAutoloadScene(scenePath);
}

void TreeRef::RemoveScene(const std::string& sceneName) const {
    if (m_Manager) m_Manager->RemoveAutoloadScene(sceneName);
}

} // namespace scripting
} // namespace lupine
