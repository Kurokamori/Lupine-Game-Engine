#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/profiling/Profiler.hpp"
#include <algorithm>

namespace lupine {
namespace core {

uint64_t Node::s_TreeStructureVersion = 0;
uint64_t Node::s_RenderEpoch = 1;

uint64_t Node::GetTreeStructureVersion() {
    return s_TreeStructureVersion;
}

uint64_t Node::GetRenderEpoch() {
    return s_RenderEpoch;
}

void Node::MarkRenderDirty() {
    ++s_RenderEpoch;
}

Node::Node()
    : m_UUID(), m_Name("Node"), m_Parent(nullptr), m_Scene(nullptr),
      m_Active(true), m_Visible(true), m_Ready(false), m_UniqueNameInOwner(false) {
}

Node::Node(const std::string& name)
    : m_UUID(), m_Name(name), m_Parent(nullptr), m_Scene(nullptr),
      m_Active(true), m_Visible(true), m_Ready(false), m_UniqueNameInOwner(false) {
}

Node::~Node() {

    if (!m_Children.empty()) {
        // The children outlive this node only if held elsewhere; detaching them
        // changes their ancestor chain, so signal it like any other reparent.
        BumpTreeStructureVersion();
    }
    for (auto& child : m_Children) {
        child->m_Parent = nullptr;
    }
    m_Children.clear();

    for (auto& component : m_Components) {
        component->DispatchDestroy();
        // Scripts and the editor hold components (ComponentRef, the inspector) beyond the
        // lifetime of the owning node. Leaving m_Owner pointing at this half-destroyed Node
        // turns any later call through such a handle into a use-after-free.
        component->SetOwner(nullptr);
    }
    m_Components.clear();
}

void Node::RegisterProperties() {

    m_Properties.clear();

    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });

    RegisterProperty<bool>("active", core::PropertyType::Bool,
        [this]() { return m_Active; },
        [this](const bool& value) { m_Active = value; });

    RegisterProperty<bool>("visible", core::PropertyType::Bool,
        [this]() { return m_Visible; },
        [this](const bool& value) { m_Visible = value; });

    RegisterProperty<bool>("unique_name_in_owner", core::PropertyType::Bool,
        [this]() { return m_UniqueNameInOwner; },
        [this](const bool& value) { m_UniqueNameInOwner = value; });
}

nlohmann::json Node::Serialize() const {
    nlohmann::json json = core::ISerializable::Serialize();

    json["uuid"] = m_UUID.ToString();

    nlohmann::json componentsJson = nlohmann::json::array();
    for (const auto& component : m_Components) {
        componentsJson.push_back(component->Serialize());
    }
    json["components"] = componentsJson;

    nlohmann::json childrenJson = nlohmann::json::array();
    for (const auto& child : m_Children) {
        childrenJson.push_back(child->Serialize());
    }
    json["children"] = childrenJson;

    if (!m_Groups.empty()) {
        nlohmann::json groupsJson = nlohmann::json::array();
        for (const std::string& group : m_Groups) {
            groupsJson.push_back(group);
        }
        json["groups"] = groupsJson;
    }

    // Persist declarative (object+method) signal connections originating from
    // this node. Targets are addressed by stable UUID with a path fallback.
    SignalObject::TargetResolveFn resolver = [](SignalObject* target) -> SignalObject::TargetLocator {
        if (Node* n = dynamic_cast<Node*>(target)) {
            return { n->GetUUID().ToString(), n->GetPath() };
        }
        return {};
    };
    nlohmann::json connections = SerializeConnections(resolver);
    if (!connections.empty()) {
        json["connections"] = connections;
    }

    return json;
}

void Node::Deserialize(const nlohmann::json& json) {

    RegisterProperties();

    core::ISerializable::Deserialize(json);

    if (json.contains("uuid")) {
        m_UUID = core::UUID::FromString(json["uuid"].get<std::string>());
    }

    if (json.contains("components") && json["components"].is_array()) {

        for (auto& component : m_Components) {
            component->DispatchDestroy();
            component->SetOwner(nullptr);
        }
        m_Components.clear();

        for (const auto& componentJson : json["components"]) {
            if (componentJson.contains("type")) {
                std::string typeName = componentJson["type"].get<std::string>();
                auto component = std::dynamic_pointer_cast<Component>(
                    core::TypeRegistry::GetInstance().CreateInstance(typeName));
                if (component) {
                    component->RegisterProperties();
                    component->Deserialize(componentJson);
                    AddComponent(component);
                }
            }
        }
    }

    if (json.contains("children") && json["children"].is_array()) {

        for (auto& child : m_Children) {
            child->m_Parent = nullptr;
            child->SetScene(nullptr);
        }
        m_Children.clear();

        for (const auto& childJson : json["children"]) {
            if (childJson.contains("type")) {
                std::string typeName = childJson["type"].get<std::string>();
                auto child = std::dynamic_pointer_cast<Node>(
                    core::TypeRegistry::GetInstance().CreateInstance(typeName));
                if (child) {
                    child->Deserialize(childJson);
                    AddChild(child);
                }
            }
        }
    }

    if (json.contains("groups") && json["groups"].is_array()) {
        m_Groups.clear();
        for (const auto& groupJson : json["groups"]) {
            if (groupJson.is_string()) {
                m_Groups.insert(groupJson.get<std::string>());
            }
        }
    }

    // Stash declarative connections; targets are resolved after the whole tree
    // is built (see SceneManager::ResolveConnectionsRecursive).
    if (json.contains("connections")) {
        LoadConnectionData(json["connections"]);
    }
}

void Node::AddChild(std::shared_ptr<Node> child) {
    if (!child) {

        return;
    }

    if (child->m_Parent) {

        child->m_Parent->RemoveChild(child);
    }

    m_Children.push_back(child);
    child->m_Parent = this;
    child->SetScene(m_Scene);
    BumpTreeStructureVersion();

    Emit("child_entered_tree", { NodeArg(child.get()) });

    // A child attached to an already-ready node (i.e. added at runtime, after the
    // scene was initialized) must run its own ready lifecycle now. Mirror the
    // scene-load order: resolve declarative signal connections across the new
    // subtree first (targets are now addressable via the scene), then run OnReady.
    // Both steps are idempotent and recurse into descendants and components.
    if (m_Ready) {
        child->ResolvePendingConnectionsRecursive();
        child->OnReady();
    }
}

void Node::InsertChild(std::shared_ptr<Node> child, size_t index) {
    if (!child) {

        return;
    }

    if (child->m_Parent) {
        child->m_Parent->RemoveChild(child);
    }

    if (index > m_Children.size()) {
        index = m_Children.size();
    }

    m_Children.insert(m_Children.begin() + index, child);
    child->m_Parent = this;
    child->SetScene(m_Scene);
    BumpTreeStructureVersion();

    Emit("child_entered_tree", { NodeArg(child.get()) });

    // See AddChild: resolve the new subtree's pending connections, then ready it.
    if (m_Ready) {
        child->ResolvePendingConnectionsRecursive();
        child->OnReady();
    }
}

void Node::RemoveChild(std::shared_ptr<Node> child) {
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        // Notify while the child is still attached and addressable.
        Emit("child_exiting_tree", { NodeArg(child.get()) });
        (*it)->m_Parent = nullptr;
        (*it)->SetScene(nullptr);
        m_Children.erase(it);
        BumpTreeStructureVersion();
    }
}

void Node::RemoveChild(const std::string& name) {
    auto child = GetChild(name);
    if (child) {
        RemoveChild(child);
    }
}

void Node::MoveChild(const std::shared_ptr<Node>& child, size_t newIndex) {
    if (!child) {
        return;
    }
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it == m_Children.end()) {
        return;
    }
    m_Children.erase(it);
    if (newIndex > m_Children.size()) {
        newIndex = m_Children.size();
    }
    m_Children.insert(m_Children.begin() + newIndex, child);
}

int Node::GetChildIndex(const Node* child) const {
    for (size_t i = 0; i < m_Children.size(); ++i) {
        if (m_Children[i].get() == child) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Node::GetIndexInParent() const {
    if (!m_Parent) {
        return -1;
    }
    return m_Parent->GetChildIndex(this);
}

void Node::SetSiblingIndex(size_t index) {
    if (!m_Parent) {
        return;
    }
    std::shared_ptr<Node> thisShared;
    for (const auto& child : m_Parent->m_Children) {
        if (child.get() == this) {
            thisShared = child;
            break;
        }
    }
    if (thisShared) {
        m_Parent->MoveChild(thisShared, index);
    }
}

void Node::ReparentTo(Node* newParent) {

    if (m_Parent == newParent) {
        return;
    }

    std::shared_ptr<Node> thisShared = nullptr;
    if (m_Parent) {

        for (const auto& child : m_Parent->m_Children) {
            if (child.get() == this) {
                thisShared = child;
                break;
            }
        }

        auto it = std::find(m_Parent->m_Children.begin(), m_Parent->m_Children.end(), thisShared);
        if (it != m_Parent->m_Children.end()) {
            m_Parent->m_Children.erase(it);
        }
    }

    m_Parent = newParent;

    if (newParent) {
        if (thisShared) {
            newParent->m_Children.push_back(thisShared);
        }
        SetScene(newParent->m_Scene);
    } else {
        SetScene(nullptr);
    }

    BumpTreeStructureVersion();

    // Moving into an already-ready parent at runtime must run this node's ready
    // lifecycle if it hasn't run yet. Resolve the moved subtree's pending
    // connections first, then ready it; both are idempotent and recurse.
    if (newParent && newParent->m_Ready) {
        ResolvePendingConnectionsRecursive();
        OnReady();
    }
}

std::shared_ptr<Node> Node::GetChild(const std::string& name) const {
    for (const auto& child : m_Children) {
        if (child->m_Name == name) {
            return child;
        }
    }
    return nullptr;
}

std::shared_ptr<Node> Node::GetChild(size_t index) const {
    if (index < m_Children.size()) {
        return m_Children[index];
    }
    return nullptr;
}

void Node::SetParent(Node* parent) {
    if (m_Parent == parent) return;

    if (m_Parent) {

        auto& siblings = m_Parent->m_Children;
        auto it = std::find_if(siblings.begin(), siblings.end(),
            [this](const std::shared_ptr<Node>& node) { return node.get() == this; });
        if (it != siblings.end()) {
            siblings.erase(it);
        }
    }

    m_Parent = parent;
    BumpTreeStructureVersion();
}

std::shared_ptr<Node> Node::GetParentShared() const {
    if (!m_Parent) return nullptr;

    // Use enable_shared_from_this to get a proper shared_ptr
    // The parent must have been created as a shared_ptr for this to work
    try {
        return const_cast<Node*>(m_Parent)->shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        // Parent wasn't created with shared_ptr, return nullptr
        return nullptr;
    }
}

std::shared_ptr<Node> Node::FindNode(const std::string& path) const {
    if (path.empty()) return nullptr;

    if (path.find('/') == std::string::npos) {
        return GetChild(path);
    }

    size_t pos = 0;
    std::shared_ptr<Node> current = nullptr;

    while (pos < path.length()) {
        size_t nextSlash = path.find('/', pos);
        std::string segment = (nextSlash == std::string::npos)
            ? path.substr(pos)
            : path.substr(pos, nextSlash - pos);

        if (segment.empty()) {
            pos = nextSlash + 1;
            continue;
        }

        if (!current) {
            current = GetChild(segment);
        } else {
            current = current->GetChild(segment);
        }

        if (!current) return nullptr;

        if (nextSlash == std::string::npos) break;
        pos = nextSlash + 1;
    }

    return current;
}

std::string Node::GetPath() const {
    if (!m_Parent) {
        return "/" + m_Name;
    }
    return m_Parent->GetPath() + "/" + m_Name;
}

Node* Node::GetUniqueNameScopeRoot() const {
    const Node* current = this;

    while (current->m_Parent) {
        if (current->m_Parent->IsSceneInstanceBoundary()) {
            return const_cast<Node*>(current);
        }
        current = current->m_Parent;
    }

    return const_cast<Node*>(current);
}

Node* Node::FindUniqueNameInScope(Node* root, const std::string& name) {
    if (!root) {
        return nullptr;
    }

    if (root->m_UniqueNameInOwner && root->m_Name == name) {
        return root;
    }

    // A SceneInstance node belongs to the current scope and is matched above,
    // but its instanced contents form their own scope and are not searched here.
    if (root->IsSceneInstanceBoundary()) {
        return nullptr;
    }

    for (const auto& child : root->m_Children) {
        if (!child) {
            continue;
        }

        Node* found = FindUniqueNameInScope(child.get(), name);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

Node* Node::ResolveUniqueName(const std::string& name) const {
    if (name.empty()) {
        return nullptr;
    }

    Node* scopeRoot = GetUniqueNameScopeRoot();
    return FindUniqueNameInScope(scopeRoot, name);
}

void Node::AddComponent(std::shared_ptr<Component> component) {
    if (!component) {

        return;
    }

    m_Components.push_back(component);
    component->SetOwner(this);

    if (m_Ready) {
        component->OnAwake();
        component->OnReady();
    }

}

void Node::RemoveComponent(std::shared_ptr<Component> component) {
    auto it = std::find(m_Components.begin(), m_Components.end(), component);
    if (it != m_Components.end()) {
        (*it)->DispatchDestroy();
        (*it)->SetOwner(nullptr);
        m_Components.erase(it);

    }
}

std::shared_ptr<Component> Node::GetComponent(const std::string& typeName) const {
    for (const auto& component : m_Components) {
        if (component->GetTypeName() == typeName) {
            return component;
        }
    }
    return nullptr;
}

void Node::SetActive(bool active) {
    if (m_Active == active) return;
    m_Active = active;
    MarkRenderDirty();

    if (!active) {
        return;
    }

    // A node saved inactive (a pause menu, say) never ran OnReady, so m_Ready is false and
    // neither it, its components, nor its subtree have been readied. Activating it has to run
    // the ready pass now - OnReady() is idempotent and recurses into the children itself.
    if (!m_Ready) {
        // Only once the node is actually part of a readied tree: a node activated before its
        // parent is ready gets readied by the parent's own OnReady pass.
        if (!m_Parent || m_Parent->IsReady()) {
            OnReady();
        }
        return;
    }

    // Already ready: children added or activated while inactive still need their ready pass.
    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child && child->IsActive()) {
            child->OnReady();
        }
    }
}

bool Node::IsActiveInHierarchy() const {
    if (!m_Active) return false;
    if (!m_Parent) return m_Active;
    return m_Parent->IsActiveInHierarchy();
}

bool Node::IsVisibleInHierarchy() const {
    if (!m_Visible) return false;
    if (!m_Parent) return m_Visible;
    return m_Parent->IsVisibleInHierarchy();
}

void Node::SetName(const std::string& name) {
    if (m_Name == name) {
        return;
    }
    m_Name = name;
    Emit("renamed");
}

void Node::SetVisible(bool visible) {
    if (m_Visible == visible) {
        return;
    }
    m_Visible = visible;
    MarkRenderDirty();
    Emit("visibility_changed", { visible });
}

nlohmann::json Node::NodeArg(const Node* node) {
    if (!node) {
        return nlohmann::json(nullptr);
    }
    return nlohmann::json{{"__lupine_node__", node->GetUUID().ToString()}};
}

SignalObject* Node::ResolveSignalOwner(const std::string& signal) {
    // The node's own lifecycle signals always win, so `ready`/`renamed`/... can
    // never be shadowed by a component that happens to declare the same name.
    if (HasOwnSignal(signal)) {
        return this;
    }
    for (const std::shared_ptr<Component>& component : m_Components) {
        if (component && component->HasOwnSignal(signal)) {
            return component.get();
        }
    }
    // Nobody declares it: keep it on the node, where an undeclared signal is
    // created on demand (a script may emit and connect its own ad-hoc name).
    return this;
}

void Node::DefineSignals() {
    // Built-in node lifecycle signals (Godot parity). Components and scripts add
    // their own signals on top of these via DefineSignals / @signal / AddUserSignal.
    RegisterSignal({"ready", {}, "Emitted once the node and its children are ready."});
    RegisterSignal({"tree_entered", {}, "Emitted when the node enters the scene tree."});
    RegisterSignal({"tree_exiting", {}, "Emitted when the node is about to leave the scene tree."});
    RegisterSignal({"renamed", {}, "Emitted when the node's name changes."});
    RegisterSignal({"visibility_changed",
                    {{"visible", PropertyValueType::Bool}},
                    "Emitted when the node's visibility changes."});
    RegisterSignal({"child_entered_tree",
                    {{"child", PropertyValueType::NodePath}},
                    "Emitted when a child node is added."});
    RegisterSignal({"child_exiting_tree",
                    {{"child", PropertyValueType::NodePath}},
                    "Emitted when a child node is about to be removed."});
}

bool Node::InvokeSignalMethod(const std::string& method, const SignalArgs& args) {
    // A connection that targets a node resolves to a handler on the node's
    // script component(s). Fan out to every component; the first that handles
    // the method satisfies the call (matching "the node's attached script").
    bool handled = false;
    for (const auto& component : m_Components) {
        if (component && component->InvokeSignalMethod(method, args)) {
            handled = true;
        }
    }
    return handled;
}

void Node::ResolvePendingConnections() {
    auto resolveTarget = [this](const PendingConnection& pc) -> Node* {
        Scene* scene = m_Scene;
        if (!pc.targetUuid.empty() && scene) {
            std::shared_ptr<Node> found = scene->FindNodeByUUID(UUID::FromString(pc.targetUuid));
            if (found) {
                return found.get();
            }
        }
        if (!pc.targetPath.empty()) {
            if (scene) {
                std::shared_ptr<Node> found = scene->FindNode(pc.targetPath);
                if (found) {
                    return found.get();
                }
            }
            std::shared_ptr<Node> rel = FindNode(pc.targetPath);
            if (rel) {
                return rel.get();
            }
        }
        return nullptr;
    };

    auto connectAll = [&resolveTarget](SignalObject* source) {
        for (const PendingConnection& pc : source->GetPendingConnections()) {
            Node* target = resolveTarget(pc);
            if (target) {
                source->Connect(pc.signal, target, pc.method, pc.flags, pc.binds);
            } else {
                LOG_WARN(LogCategory::Core,
                         "Unresolved signal connection '{}' -> '{}' (uuid='{}', path='{}')",
                         pc.signal, pc.method, pc.targetUuid, pc.targetPath);
            }
        }
        source->ClearPendingConnections();
    };

    connectAll(this);
    for (const auto& component : m_Components) {
        if (component) {
            connectAll(component.get());
        }
    }
}

void Node::ResolvePendingConnectionsRecursive() {
    ResolvePendingConnections();
    for (const auto& child : m_Children) {
        if (child) {
            child->ResolvePendingConnectionsRecursive();
        }
    }
}

void Node::SetScene(Scene* scene) {
    Scene* previous = m_Scene;
    m_Scene = scene;

    const bool entered = (previous == nullptr && scene != nullptr);
    const bool exiting = (previous != nullptr && scene == nullptr);

    // tree_entered propagates top-down (this node before its children); the
    // emit on a node with no listeners returns immediately.
    if (entered) {
        Emit("tree_entered");
    }

    for (auto& child : m_Children) {
        child->SetScene(scene);
    }

    // tree_exiting propagates bottom-up (children leave before this node).
    if (exiting) {
        Emit("tree_exiting");
    }
}

void Node::OnReady() {
    if (m_Ready) return;
    m_Ready = true;

    std::shared_ptr<Node> selfGuard = shared_from_this();

    std::vector<std::shared_ptr<Component>> components = m_Components;
    for (std::shared_ptr<Component>& component : components) {
        component->OnAwake();
        component->OnReady();
    }

    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child && child->IsActive()) {
            child->OnReady();
        }
    }

    Emit("ready");
}

void Node::OnDestroy() {

    std::shared_ptr<Node> selfGuard = shared_from_this();

    std::vector<std::shared_ptr<Component>> components = m_Components;
    for (std::shared_ptr<Component>& component : components) {
        component->DispatchDestroy();
    }

    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child) {
            child->OnDestroy();
        }
    }
}

void Node::OnInput(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    std::shared_ptr<Node> selfGuard = shared_from_this();

    Component* inputController = nullptr;
    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        component->OnInput(deltaTime);

        if (!inputController && component->IsEnabled() && component->WantsChildInputControl()) {
            inputController = component.get();
        }
    }

    if (inputController) {
        // The controller receives a snapshot: an input handler may add or remove children.
        std::vector<std::shared_ptr<Node>> children = m_Children;
        inputController->DispatchChildInput(deltaTime, children);
    } else {
        for (size_t i = 0; i < m_Children.size(); ++i) {
            std::shared_ptr<Node> child = m_Children[i];
            if (child) {
                child->OnInput(deltaTime);
            }
        }
    }

}

void Node::OnInputEvent(const nlohmann::json& event) {
    if (!IsActiveInHierarchy()) return;

    std::shared_ptr<Node> selfGuard = shared_from_this();

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        component->OnInputEvent(event);
    }

    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child) {
            child->OnInputEvent(event);
        }
    }
}

void Node::AddToGroup(const std::string& group) {
    if (!group.empty()) {
        m_Groups.insert(group);
    }
}

void Node::RemoveFromGroup(const std::string& group) {
    m_Groups.erase(group);
}

bool Node::IsInGroup(const std::string& group) const {
    return m_Groups.find(group) != m_Groups.end();
}

std::vector<std::string> Node::GetGroups() const {
    std::vector<std::string> groups;
    groups.reserve(m_Groups.size());
    for (const std::string& group : m_Groups) {
        groups.push_back(group);
    }
    return groups;
}

std::vector<std::string> Node::GetDeclaredInterfaces() const {
    std::vector<std::string> declared;
    std::unordered_set<std::string> seen;
    for (const std::shared_ptr<Component>& component : m_Components) {
        if (!component) {
            continue;
        }
        for (const std::string& iface : component->GetImplementedInterfaces()) {
            if (seen.insert(iface).second) {
                declared.push_back(iface);
            }
        }
    }
    return declared;
}

std::vector<std::string> Node::GetImplementedInterfaces() const {
    return InterfaceRegistry::GetInstance().ExpandImplied(GetDeclaredInterfaces());
}

bool Node::ImplementsInterface(const std::string& interfaceName) const {
    InterfaceRegistry& registry = InterfaceRegistry::GetInstance();
    for (const std::shared_ptr<Component>& component : m_Components) {
        if (!component) {
            continue;
        }
        for (const std::string& declared : component->GetImplementedInterfaces()) {
            if (registry.IsSubInterfaceOf(declared, interfaceName)) {
                return true;
            }
        }
    }
    return false;
}

nlohmann::json Node::VerifyInterface(const std::string& interfaceName) const {
    InterfaceRegistry& registry = InterfaceRegistry::GetInstance();

    nlohmann::json result;
    result["interface"] = interfaceName;

    const InterfaceDefinition* def = registry.GetDefinition(interfaceName);
    if (!def) {
        result["exists"] = false;
        result["conforms"] = false;
        result["missing_methods"] = nlohmann::json::array();
        result["missing_signals"] = nlohmann::json::array();
        return result;
    }
    result["exists"] = true;

    nlohmann::json missingMethods = nlohmann::json::array();
    for (const InterfaceMethod& method : registry.GetEffectiveMethods(interfaceName)) {
        bool found = false;
        for (const std::shared_ptr<Component>& component : m_Components) {
            if (component && component->HasInterfaceMethod(method.name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            missingMethods.push_back(method.name);
        }
    }

    nlohmann::json missingSignals = nlohmann::json::array();
    for (const SignalDesc& sig : registry.GetEffectiveSignals(interfaceName)) {
        bool found = HasSignal(sig.name);
        if (!found) {
            for (const std::shared_ptr<Component>& component : m_Components) {
                if (component && component->HasSignal(sig.name)) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            missingSignals.push_back(sig.name);
        }
    }

    result["missing_methods"] = missingMethods;
    result["missing_signals"] = missingSignals;
    result["conforms"] = missingMethods.empty() && missingSignals.empty();
    return result;
}

void Node::OnProcess(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    // A component's script may destroy this node (parent.remove_child(self.node)) from
    // inside its callback. Keeping ourselves alive for the duration means control returns
    // into a live object rather than freed memory.
    std::shared_ptr<Node> selfGuard = shared_from_this();

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        component->OnUpdate(deltaTime);
    }

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        LUPINE_PROFILE_ACCUM("comp.process." + component->GetTypeName(),
                             component->OnProcess(deltaTime));
    }

    // Index-based with a per-iteration copy: scripts add and remove children synchronously
    // (create_node(parent=root), remove_child), so the vector can grow, shrink or reallocate
    // while this loop runs.
    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child) {
            child->OnProcess(deltaTime);
        }
    }

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        component->OnLateUpdate(deltaTime);
    }

}

void Node::OnPhysicsProcess(float deltaTime) {
    if (!IsActiveInHierarchy()) return;

    std::shared_ptr<Node> selfGuard = shared_from_this();

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        LUPINE_PROFILE_ACCUM("comp.physics." + component->GetTypeName(),
                             component->OnPhysicsProcess(deltaTime));
    }

    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child) {
            child->OnPhysicsProcess(deltaTime);
        }
    }

}

void Node::OnRender() {
    if (!IsVisibleInHierarchy()) return;

    std::shared_ptr<Node> selfGuard = shared_from_this();

    for (size_t i = 0; i < m_Components.size(); ++i) {
        std::shared_ptr<Component> component = m_Components[i];
        LUPINE_PROFILE_ACCUM("comp.render." + component->GetTypeName(),
                             component->OnRender());
    }

    for (size_t i = 0; i < m_Children.size(); ++i) {
        std::shared_ptr<Node> child = m_Children[i];
        if (child) {
            child->OnRender();
        }
    }

}

Node2D::Node2D()
    : Node(), m_Position(0.0f, 0.0f), m_Rotation(0.0f),
      m_Scale(1.0f, 1.0f), m_ZIndex(0) {
}

Node2D::Node2D(const std::string& name)
    : Node(name), m_Position(0.0f, 0.0f), m_Rotation(0.0f),
      m_Scale(1.0f, 1.0f), m_ZIndex(0) {
}

void Node2D::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<math::Vec2>("position", core::PropertyType::Vec2,
        [this]() { return m_Position; },
        [this](const math::Vec2& value) { SetPosition(value); });

    RegisterProperty<float>("rotation", core::PropertyType::Float,
        [this]() { return m_Rotation; },
        [this](const float& value) { SetRotation(value); });

    RegisterProperty<math::Vec2>("scale", core::PropertyType::Vec2,
        [this]() { return m_Scale; },
        [this](const math::Vec2& value) { SetScale(value); });

    RegisterProperty<int>("z_index", core::PropertyType::Int,
        [this]() { return m_ZIndex; },
        [this](const int& value) { SetZIndex(value); });
}

math::Vec2 Node2D::GetGlobalPosition() const {
    if (!m_Parent) return m_Position;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        // Transform the local position by the parent's global matrix, which is exactly what
        // GetGlobalTransformMatrix() (and therefore rendering) does. Recomposing it from the
        // aggregated global position/rotation/scale instead produced a different point
        // whenever a rotated ancestor had a non-uniform scale - the scale was applied after
        // the rotation rather than before, and an ancestor chain can carry shear that no
        // single (rotation, scale) pair can represent. Physics and scripts read this getter,
        // so any divergence puts gameplay and visuals in different places.
        const math::Mat4 parentGlobal = parent2D->GetGlobalTransformMatrix();
        const math::Vec4 global = parentGlobal * math::Vec4(m_Position.x, m_Position.y, 0.0f, 1.0f);
        return math::Vec2(global.x, global.y);
    }

    return m_Position;
}

float Node2D::GetGlobalRotation() const {
    if (!m_Parent) return m_Rotation;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        return parent2D->GetGlobalRotation() + m_Rotation;
    }

    return m_Rotation;
}

math::Vec2 Node2D::GetGlobalScale() const {
    if (!m_Parent) return m_Scale;

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        math::Vec2 parentScale = parent2D->GetGlobalScale();
        return m_Scale * parentScale;
    }

    return m_Scale;
}

math::Mat4 Node2D::GetTransformMatrix() const {

    math::Mat4 translation = math::Mat4::Translate(math::Vec3(m_Position.x, m_Position.y, 0.0f));
    math::Mat4 rotation = math::Mat4::Rotate(m_Rotation, math::Vec3(0.0f, 0.0f, 1.0f));
    math::Mat4 scale = math::Mat4::Scale(math::Vec3(m_Scale.x, m_Scale.y, 1.0f));
    return translation * rotation * scale;
}

math::Mat4 Node2D::GetGlobalTransformMatrix() const {
    if (!m_Parent) return GetTransformMatrix();

    Node2D* parent2D = dynamic_cast<Node2D*>(m_Parent);
    if (parent2D) {
        return parent2D->GetGlobalTransformMatrix() * GetTransformMatrix();
    }

    return GetTransformMatrix();
}

Node3D::Node3D()
    : Node(), m_Position(0.0f, 0.0f, 0.0f), m_Rotation(),
      m_Scale(1.0f, 1.0f, 1.0f) {
}

Node3D::Node3D(const std::string& name)
    : Node(name), m_Position(0.0f, 0.0f, 0.0f), m_Rotation(),
      m_Scale(1.0f, 1.0f, 1.0f) {
}

void Node3D::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<math::Vec3>("position", core::PropertyType::Vec3,
        [this]() { return m_Position; },
        [this](const math::Vec3& value) { SetPosition(value); });

    RegisterProperty<math::Quat>("rotation", core::PropertyType::Vec4,
        [this]() { return m_Rotation; },
        [this](const math::Quat& value) { SetRotation(value); });

    RegisterProperty<math::Vec3>("scale", core::PropertyType::Vec3,
        [this]() { return m_Scale; },
        [this](const math::Vec3& value) { SetScale(value); });
}

math::Vec3 Node3D::GetGlobalPosition() const {
    if (!m_Parent) return m_Position;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        math::Mat4 parentTransform = parent3D->GetGlobalTransformMatrix();
        math::Vec4 pos4(m_Position.x, m_Position.y, m_Position.z, 1.0f);
        math::Vec4 globalPos4 = parentTransform * pos4;
        return math::Vec3(globalPos4.x, globalPos4.y, globalPos4.z);
    }

    return m_Position;
}

math::Quat Node3D::GetGlobalRotation() const {
    if (!m_Parent) return m_Rotation;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        return parent3D->GetGlobalRotation() * m_Rotation;
    }

    return m_Rotation;
}

math::Vec3 Node3D::GetGlobalScale() const {
    if (!m_Parent) return m_Scale;

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        math::Vec3 parentScale = parent3D->GetGlobalScale();
        return m_Scale * parentScale;
    }

    return m_Scale;
}

math::Mat4 Node3D::GetTransformMatrix() const {
    math::Mat4 translation = math::Mat4::Translate(m_Position);
    math::Mat4 rotation = m_Rotation.ToMat4();
    math::Mat4 scale = math::Mat4::Scale(m_Scale);
    return translation * rotation * scale;
}

math::Mat4 Node3D::GetGlobalTransformMatrix() const {
    if (!m_Parent) return GetTransformMatrix();

    Node3D* parent3D = dynamic_cast<Node3D*>(m_Parent);
    if (parent3D) {
        return parent3D->GetGlobalTransformMatrix() * GetTransformMatrix();
    }

    return GetTransformMatrix();
}

}
}

