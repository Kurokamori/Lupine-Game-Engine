#include "lupine/components/UIControl.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/input/InputManager.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_set>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

UIControl::UIControl()
    : Component("UIControl")
{
}

UIControl::UIControl(const std::string& name)
    : Component(name)
{
}

namespace {
    UIControl* s_FocusedControl = nullptr;
}

uint64_t UIControl::s_ThemeOwnerGeneration = 0;

uint64_t UIControl::s_PointerCacheFrame = ~0ull;
Vec2 UIControl::s_PointerCachePoint = Vec2(0.0f, 0.0f);
UIControl* UIControl::s_PointerCacheTarget = nullptr;
bool UIControl::s_PointerCacheValid = false;

UIControl::~UIControl() {
    if (s_FocusedControl == this) {
        s_FocusedControl = nullptr;
    }
}

void UIControl::GrabFocus() {
    s_FocusedControl = this;
}

void UIControl::ReleaseFocus() {
    if (s_FocusedControl == this) {
        s_FocusedControl = nullptr;
    }
}

bool UIControl::HasFocus() const {
    return s_FocusedControl == this;
}

UIControl* UIControl::GetFocusedControl() {
    return s_FocusedControl;
}

void UIControl::ClearFocus() {
    s_FocusedControl = nullptr;
}

// ========================================
// Focus mode + neighbor navigation
// ========================================

const char* UIControl::FocusNeighborPropertyName(FocusSide side) {
    switch (side) {
        case FocusSide::Left:   return "focusNeighborLeft";
        case FocusSide::Top:    return "focusNeighborTop";
        case FocusSide::Right:  return "focusNeighborRight";
        case FocusSide::Bottom: return "focusNeighborBottom";
    }
    return "focusNeighborLeft";
}

FocusMode UIControl::GetFocusMode() const {
    return static_cast<FocusMode>(GetPropertyValue<int>("focusMode"));
}

void UIControl::SetFocusMode(FocusMode mode) {
    SetPropertyValue<int>("focusMode", static_cast<int>(mode));
    if (mode == FocusMode::None && HasFocus()) {
        ReleaseFocus();
    }
}

bool UIControl::CanGrabFocus() const {
    if (GetFocusMode() != FocusMode::All) {
        return false;
    }
    const Node* owner = GetOwner();
    if (!owner) {
        return false;
    }
    return owner->IsActiveInHierarchy() && owner->IsVisibleInHierarchy();
}

std::string UIControl::GetFocusNeighbor(FocusSide side) const {
    return GetPropertyValue<std::string>(FocusNeighborPropertyName(side));
}

void UIControl::SetFocusNeighbor(FocusSide side, const std::string& nodePath) {
    SetPropertyValue<std::string>(FocusNeighborPropertyName(side), nodePath);
}

std::string UIControl::GetFocusNext() const {
    return GetPropertyValue<std::string>("focusNext");
}

void UIControl::SetFocusNext(const std::string& nodePath) {
    SetPropertyValue<std::string>("focusNext", nodePath);
}

std::string UIControl::GetFocusPrevious() const {
    return GetPropertyValue<std::string>("focusPrevious");
}

void UIControl::SetFocusPrevious(const std::string& nodePath) {
    SetPropertyValue<std::string>("focusPrevious", nodePath);
}

UIControl* UIControl::ResolveNeighborControl(const std::string& nodePath) const {
    if (nodePath.empty() || nodePath == ".") {
        return nullptr;
    }
    Node* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }
    scripting::NodeRef ref = scripting::NodeRef::FromRaw(owner, nullptr).FindNode(nodePath);
    Node* target = ref.IsValid() ? ref.Lock().get() : nullptr;
    if (!target) {
        return nullptr;
    }
    std::shared_ptr<UIControl> control = target->GetComponent<UIControl>();
    return control.get();
}

void UIControl::CollectFocusableControls(std::vector<UIControl*>& out) const {
    const Node* owner = GetOwner();
    if (!owner) {
        return;
    }

    // Determine the traversal root: the owning scene's root, else the top-most
    // ancestor (covers editor / detached trees with no Scene attached).
    Node* root = nullptr;
    if (Scene* scene = owner->GetScene()) {
        root = scene->GetRoot().get();
    }
    if (!root) {
        Node* walk = const_cast<Node*>(owner);
        while (walk->GetParent()) {
            walk = walk->GetParent();
        }
        root = walk;
    }
    if (!root) {
        return;
    }

    std::function<void(Node*)> visit = [&](Node* node) {
        if (!node) {
            return;
        }
        for (const std::shared_ptr<Component>& comp : node->GetComponents()) {
            UIControl* control = dynamic_cast<UIControl*>(comp.get());
            if (control && control->CanGrabFocus()) {
                out.push_back(control);
            }
        }
        for (const std::shared_ptr<Node>& child : node->GetChildren()) {
            visit(child.get());
        }
    };
    visit(root);
}

UIControl* UIControl::FindGeometricNeighbor(FocusSide side) const {
    std::vector<UIControl*> candidates;
    CollectFocusableControls(candidates);
    if (candidates.empty()) {
        return nullptr;
    }

    const Vec2 selfCenter = GetRect().GetCenter();

    // Direction the side points in (screen space, +y down) and the axis used to
    // penalize lateral offset so the chosen neighbor stays roughly in line.
    Vec2 dir(0.0f, 0.0f);
    switch (side) {
        case FocusSide::Left:   dir = Vec2(-1.0f, 0.0f); break;
        case FocusSide::Top:    dir = Vec2(0.0f, -1.0f); break;
        case FocusSide::Right:  dir = Vec2(1.0f, 0.0f);  break;
        case FocusSide::Bottom: dir = Vec2(0.0f, 1.0f);  break;
    }

    UIControl* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (UIControl* candidate : candidates) {
        if (candidate == this) {
            continue;
        }
        const Vec2 delta = candidate->GetRect().GetCenter() - selfCenter;
        const float along = delta.x * dir.x + delta.y * dir.y;
        if (along <= 0.0f) {
            continue;  // not in the requested direction
        }
        // Lateral distance perpendicular to the search axis.
        const float lateral = std::abs(delta.x * dir.y) + std::abs(delta.y * dir.x);
        // Prefer near + well-aligned: forward distance plus a lateral penalty.
        const float score = along + lateral * 2.0f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

UIControl* UIControl::FindValidFocusNeighbor(FocusSide side) const {
    UIControl* explicitNeighbor = ResolveNeighborControl(GetFocusNeighbor(side));
    if (explicitNeighbor) {
        return explicitNeighbor;
    }
    return FindGeometricNeighbor(side);
}

UIControl* UIControl::FindNextValidFocus() const {
    UIControl* explicitNext = ResolveNeighborControl(GetFocusNext());
    if (explicitNext) {
        return explicitNext;
    }

    std::vector<UIControl*> order;
    CollectFocusableControls(order);
    if (order.size() < 2) {
        return nullptr;
    }
    auto it = std::find(order.begin(), order.end(), this);
    if (it == order.end()) {
        return order.front();
    }
    ++it;
    if (it == order.end()) {
        it = order.begin();
    }
    return *it;
}

UIControl* UIControl::FindPrevValidFocus() const {
    UIControl* explicitPrev = ResolveNeighborControl(GetFocusPrevious());
    if (explicitPrev) {
        return explicitPrev;
    }

    std::vector<UIControl*> order;
    CollectFocusableControls(order);
    if (order.size() < 2) {
        return nullptr;
    }
    auto it = std::find(order.begin(), order.end(), this);
    if (it == order.end()) {
        return order.back();
    }
    if (it == order.begin()) {
        it = order.end();
    }
    --it;
    return *it;
}

UIControl* UIControl::GrabFocusNeighbor(FocusSide side) {
    UIControl* target = FindValidFocusNeighbor(side);
    if (target) {
        target->GrabFocus();
    }
    return target;
}

UIControl* UIControl::GrabFocusNext() {
    UIControl* target = FindNextValidFocus();
    if (target) {
        target->GrabFocus();
    }
    return target;
}

UIControl* UIControl::GrabFocusPrevious() {
    UIControl* target = FindPrevValidFocus();
    if (target) {
        target->GrabFocus();
    }
    return target;
}

// ========================================
// Pointer (mouse) input arbitration
// ========================================

bool UIControl::ContainsCanvasPoint(const Vec2& point) const {
    Rect bounds(GetRect().position, GetBoundsSize());
    return bounds.Contains(point);
}

int UIControl::GetPointerSortKey() const {
    return GetPropertyValue<int>("layer") * 1000 + GetPropertyValue<int>("sortingOrder");
}

UIControl* UIControl::GetTopPointerTarget(const Vec2& point, const Node* sceneContext) {
    // Reuse the cached winner when the same input frame + cursor position is queried
    // again (every consuming control in a frame asks for the identical cursor point).
    input::InputManager& inputMgr = input::InputManager::Get();
    const uint64_t frame = inputMgr.GetFrameNumber();
    const float eps = 0.5f;
    if (s_PointerCacheValid && frame == s_PointerCacheFrame &&
        std::abs(point.x - s_PointerCachePoint.x) <= eps &&
        std::abs(point.y - s_PointerCachePoint.y) <= eps) {
        return s_PointerCacheTarget;
    }

    Node* root = nullptr;
    if (sceneContext) {
        if (Scene* scene = sceneContext->GetScene()) {
            root = scene->GetRoot().get();
        }
        if (!root) {
            Node* walk = const_cast<Node*>(sceneContext);
            while (walk->GetParent()) {
                walk = walk->GetParent();
            }
            root = walk;
        }
    }

    UIControl* best = nullptr;
    int bestKey = 0;
    int bestOrder = -1;
    int orderCounter = 0;

    // The traversal carries the intersection of every clip rect established by the
    // ancestors above it (`hasClip` distinguishes "no clip at all" from "a clip that
    // happens to cover everything").
    //
    // ClipsDescendants()/GetClipRect() used to be consumed ONLY by the renderer, which
    // pushes them as a scissor. The pointer arbiter tested each control against its own
    // rect and never against any ancestor's clip, so a Button scrolled out of a
    // ScrollContainer's viewport was invisible yet still won arbitration and fired
    // `clicked`.
    std::function<void(Node*, bool, const Rect&)> visit =
        [&](Node* node, bool hasClip, const Rect& clip) {
        if (!node) {
            return;
        }

        // A clip that has collapsed to nothing (a viewport with no room left, or two
        // ancestor clips that do not overlap) hides its whole subtree.
        if (hasClip && (clip.size.x <= 0.0f || clip.size.y <= 0.0f)) {
            return;
        }

        // The cursor is outside this subtree's clip. Nothing below can be hit either:
        // descending only ever intersects the clip further, never widens it.
        if (hasClip && !clip.Contains(point)) {
            return;
        }

        const int nodeOrder = orderCounter++;
        for (const std::shared_ptr<Component>& comp : node->GetComponents()) {
            UIControl* control = dynamic_cast<UIControl*>(comp.get());
            if (!control || !control->ConsumesPointerInput()) {
                continue;
            }
            const Node* owner = control->GetOwner();
            if (owner && (!owner->IsActiveInHierarchy() || !owner->IsVisibleInHierarchy())) {
                continue;
            }
            if (!control->ContainsCanvasPoint(point)) {
                continue;
            }
            const int key = control->GetPointerSortKey();
            // Highest render key wins; equal keys resolve to the later-drawn control
            // (higher pre-order index), matching the renderer's draw order.
            if (!best || key > bestKey || (key == bestKey && nodeOrder > bestOrder)) {
                best = control;
                bestKey = key;
                bestOrder = nodeOrder;
            }
        }

        // Fold in any clip this node itself establishes, for its descendants only -- a
        // control does not clip its own hit rect, just as it does not scissor its own draws.
        bool childHasClip = hasClip;
        Rect childClip = clip;
        for (const std::shared_ptr<Component>& comp : node->GetComponents()) {
            UIControl* control = dynamic_cast<UIControl*>(comp.get());
            if (!control || !control->ClipsDescendants()) {
                continue;
            }
            const Rect ownClip = control->GetClipRect();
            childClip = childHasClip ? childClip.GetIntersection(ownClip) : ownClip;
            childHasClip = true;
        }

        for (const std::shared_ptr<Node>& child : node->GetChildren()) {
            visit(child.get(), childHasClip, childClip);
        }
    };
    if (root) {
        visit(root, false, Rect());
    }

    s_PointerCacheValid = true;
    s_PointerCacheFrame = frame;
    s_PointerCachePoint = point;
    s_PointerCacheTarget = best;
    return best;
}

bool UIControl::IsTopPointerTarget(const Vec2& point) const {
    // No pointer consumer (or this control is decorative): nothing to arbitrate, so
    // never block — the caller's own hit-test stands.
    if (!ConsumesPointerInput()) {
        return true;
    }
    return GetTopPointerTarget(point, GetOwner()) == this;
}

// ========================================
// Mouse filter
// ========================================

namespace {
    // The mouse buttons the filter state machine tracks (index == the bit it owns in
    // m_MousePressedMask, which is also the value the signals report).
    constexpr input::MouseButton kFilterMouseButtons[] = {
        input::MouseButton::Left,
        input::MouseButton::Right,
        input::MouseButton::Middle
    };
}

MouseFilter UIControl::GetMouseFilter() const {
    // Controls that never registered the property are decorative by definition.
    if (!GetPropertyRegistry().HasProperty("mouseFilter")) {
        return MouseFilter::Ignore;
    }
    int raw = GetPropertyValue<int>("mouseFilter");
    if (raw < static_cast<int>(MouseFilter::Stop) || raw > static_cast<int>(MouseFilter::Ignore)) {
        return MouseFilter::Ignore;
    }
    return static_cast<MouseFilter>(raw);
}

void UIControl::SetMouseFilter(MouseFilter filter) {
    if (!GetPropertyRegistry().HasProperty("mouseFilter")) {
        return;
    }
    SetPropertyValue<int>("mouseFilter", static_cast<int>(filter));
    if (filter == MouseFilter::Ignore) {
        m_MouseHovered = false;
        m_MousePressedMask = 0;
    }
}

bool UIControl::IsMousePressed(input::MouseButton button) const {
    return (m_MousePressedMask & (1u << static_cast<uint32_t>(button))) != 0u;
}

void UIControl::DefineMouseFilterProperty(MouseFilter defaultFilter, const char* group) {
    DefineProperty(PROPERTY_ENUM_GROUP(mouseFilter, static_cast<int>(defaultFilter), group,
        Stop, Propagate, Ignore));
}

void UIControl::DefineMouseFilterSignals() {
    RegisterSignal({"mouse_entered", {}, "Emitted when the cursor enters the control."});
    RegisterSignal({"mouse_exited", {}, "Emitted when the cursor leaves the control."});
    RegisterSignal({"mouse_pressed",
                    {{"button", core::PropertyValueType::Int}},
                    "Emitted when a mouse button is pressed over the control (0=left, 1=right, 2=middle)."});
    RegisterSignal({"mouse_released",
                    {{"button", core::PropertyValueType::Int}},
                    "Emitted when a mouse button pressed over the control is released."});
    RegisterSignal({"clicked",
                    {{"button", core::PropertyValueType::Int}},
                    "Emitted when a press and its release both happen over the control."});
}

void UIControl::UpdateMouseFilterState() {
    const MouseFilter filter = GetMouseFilter();

    // Ignore: invisible to the cursor. Drop any state left over from a filter change
    // mid-interaction, emitting the exit so listeners are never left thinking the
    // cursor is still inside.
    if (filter == MouseFilter::Ignore) {
        m_MousePressedMask = 0;
        if (m_MouseHovered) {
            m_MouseHovered = false;
            Emit("mouse_exited");
        }
        return;
    }

    const Node* owner = GetOwner();
    if (owner && (!owner->IsActiveInHierarchy() || !owner->IsVisibleInHierarchy())) {
        m_MousePressedMask = 0;
        if (m_MouseHovered) {
            m_MouseHovered = false;
            Emit("mouse_exited");
        }
        return;
    }

    const Vec2 mousePos = GetCanvasMousePosition();

    // A Stop control arbitrates: only the front-most one under the cursor reacts, so a
    // control behind it stays inert. A Propagate control is transparent to arbitration
    // (IsTopPointerTarget short-circuits to true for it anyway, since it is not a
    // consumer), so it reacts alongside whatever is behind it.
    const bool isOver = ContainsCanvasPoint(mousePos) && IsTopPointerTarget(mousePos);

    if (isOver != m_MouseHovered) {
        m_MouseHovered = isOver;
        Emit(isOver ? "mouse_entered" : "mouse_exited");
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    for (const input::MouseButton button : kFilterMouseButtons) {
        const uint32_t bit = 1u << static_cast<uint32_t>(button);
        const int buttonIndex = static_cast<int>(button);

        if (isOver && inputMgr.IsMouseButtonJustPressed(button)) {
            m_MousePressedMask |= bit;
            Emit("mouse_pressed", { buttonIndex });
            continue;
        }

        // Only a press that began on this control can release/click on it, and a
        // release outside the control cancels the click (standard button semantics).
        if ((m_MousePressedMask & bit) != 0u && !inputMgr.IsMouseButtonPressed(button)) {
            m_MousePressedMask &= ~bit;
            Emit("mouse_released", { buttonIndex });
            if (isOver) {
                Emit("clicked", { buttonIndex });
            }
        }
    }
}

// ========================================
// Scripting dispatch (Lua / mruby / Python / C-API)
// ========================================

nlohmann::json UIControl::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argI = [&](size_t i, int fallback) -> int {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<int>();
        }
        return fallback;
    };
    auto argStr = [&](size_t i, const std::string& fallback) -> std::string {
        if (args.is_array() && i < args.size() && args[i].is_string()) {
            return args[i].get<std::string>();
        }
        return fallback;
    };
    auto sideArg = [&](size_t i) -> FocusSide {
        int raw = argI(i, 0);
        if (raw < 0) raw = 0;
        if (raw > static_cast<int>(FocusSide::Bottom)) raw = static_cast<int>(FocusSide::Bottom);
        return static_cast<FocusSide>(raw);
    };
    auto controlNodeArg = [](UIControl* control) -> nlohmann::json {
        return Node::NodeArg(control ? control->GetOwner() : nullptr);
    };

    // ---- Layout argument/return helpers ----
    //
    // The whole layout surface was unreachable from script: CallMethod dispatched only focus
    // and mouse-filter methods, and GetRect() is not a property (it is the computed
    // m_ResolvedRect), so ComponentRef::Get's serialize fallback could not surface it either.
    // Gameplay code therefore could not ask where a control actually ended up -- which is
    // exactly what tooltips, world-to-UI attachment and custom hit regions need.
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto argB = [&](size_t i, bool fallback) -> bool {
        if (args.is_array() && i < args.size() && args[i].is_boolean()) {
            return args[i].get<bool>();
        }
        return fallback;
    };
    // Accepts a Vec2 as either two scalars, a {x=,y=} table, or a [x, y] array, so scripts
    // can pass whichever their language makes natural.
    auto argVec2 = [&](size_t i, const Vec2& fallback) -> Vec2 {
        if (!args.is_array() || i >= args.size()) {
            return fallback;
        }
        const nlohmann::json& v = args[i];
        if (v.is_number() && (i + 1) < args.size() && args[i + 1].is_number()) {
            return Vec2(v.get<float>(), args[i + 1].get<float>());
        }
        if (v.is_array() && v.size() >= 2 && v[0].is_number() && v[1].is_number()) {
            return Vec2(v[0].get<float>(), v[1].get<float>());
        }
        if (v.is_object() && v.contains("x") && v.contains("y") &&
            v["x"].is_number() && v["y"].is_number()) {
            return Vec2(v["x"].get<float>(), v["y"].get<float>());
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        return nlohmann::json{ {"x", v.x}, {"y", v.y} };
    };
    // A rect is returned with BOTH its raw min-corner form and its named canvas edges, so a
    // script never has to know that position.y is the bottom on this Y-up canvas.
    auto rectJson = [](const Rect& r) -> nlohmann::json {
        return nlohmann::json{
            {"x", r.position.x}, {"y", r.position.y},
            {"width", r.size.x}, {"height", r.size.y},
            {"left", r.GetLeftEdge()}, {"right", r.GetRightEdge()},
            {"top", r.GetTopEdge()}, {"bottom", r.GetBottomEdge()},
            {"center_x", r.GetCenter().x}, {"center_y", r.GetCenter().y}
        };
    };

    // ---- Layout: measure / arrange queries ----
    if (method == "get_rect") {
        return rectJson(GetRect());
    } else if (method == "get_size") {
        return vec2Json(GetSize());
    } else if (method == "get_bounds_size") {
        return vec2Json(GetBoundsSize());
    } else if (method == "get_desired_size") {
        return vec2Json(GetDesiredSize());
    } else if (method == "get_min_size") {
        return vec2Json(GetMinSize());
    } else if (method == "get_max_size") {
        return vec2Json(GetMaxSize());
    } else if (method == "get_content_min_size") {
        return vec2Json(GetContentMinSize());
    } else if (method == "is_container_driven") {
        // Whether a parent container owns this control's rect -- i.e. whether writing
        // width/height/anchors/offsets from script will be overwritten on the next layout.
        return IsContainerDriven();
    } else if (method == "is_layout_container") {
        return IsLayoutContainer();

    // ---- Layout: mutators ----
    } else if (method == "set_size") {
        SetSize(argVec2(0, GetSize()));
        return nlohmann::json();
    } else if (method == "set_width") {
        SetWidth(argF(0, GetWidth()));
        return nlohmann::json();
    } else if (method == "set_height") {
        SetHeight(argF(0, GetHeight()));
        return nlohmann::json();
    } else if (method == "set_custom_min_size") {
        SetCustomMinSize(argVec2(0, GetCustomMinSize()));
        return nlohmann::json();
    } else if (method == "set_custom_max_size") {
        SetCustomMaxSize(argVec2(0, GetCustomMaxSize()));
        return nlohmann::json();
    } else if (method == "get_anchor_preset") {
        return static_cast<int>(GetAnchorPreset());
    } else if (method == "set_anchor_preset") {
        int raw = argI(0, static_cast<int>(AnchorPreset::Custom));
        if (raw < 0) raw = 0;
        if (raw > static_cast<int>(AnchorPreset::Custom)) raw = static_cast<int>(AnchorPreset::Custom);
        // Second arg keeps the control's current on-screen rect instead of snapping it into
        // the preset's natural region.
        ApplyAnchorPreset(static_cast<AnchorPreset>(raw), argB(1, false));
        return nlohmann::json();
    } else if (method == "get_anchors") {
        return nlohmann::json{ {"min", vec2Json(GetAnchorMin())}, {"max", vec2Json(GetAnchorMax())} };
    } else if (method == "set_anchors") {
        // set_anchors(minX, minY, maxX, maxY) or set_anchors({x,y}, {x,y}).
        if (args.is_array() && args.size() >= 4 && args[0].is_number()) {
            SetAnchorMin(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
            SetAnchorMax(Vec2(argF(2, 0.0f), argF(3, 0.0f)));
        } else {
            SetAnchorMin(argVec2(0, GetAnchorMin()));
            SetAnchorMax(argVec2(1, GetAnchorMax()));
        }
        return nlohmann::json();
    } else if (method == "get_offsets") {
        return nlohmann::json{ {"min", vec2Json(GetOffsetMin())}, {"max", vec2Json(GetOffsetMax())} };
    } else if (method == "set_offsets") {
        if (args.is_array() && args.size() >= 4 && args[0].is_number()) {
            SetOffsetMin(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
            SetOffsetMax(Vec2(argF(2, 0.0f), argF(3, 0.0f)));
        } else {
            SetOffsetMin(argVec2(0, GetOffsetMin()));
            SetOffsetMax(argVec2(1, GetOffsetMax()));
        }
        return nlohmann::json();
    } else if (method == "get_size_flags") {
        return nlohmann::json{
            {"horizontal", GetSizeFlagsHorizontal()},
            {"vertical", GetSizeFlagsVertical()},
            {"stretch_ratio", GetSizeFlagsStretchRatio()}
        };
    } else if (method == "set_size_flags") {
        // set_size_flags(horizontal, vertical[, stretchRatio])
        SetSizeFlagsHorizontal(argI(0, GetSizeFlagsHorizontal()));
        SetSizeFlagsVertical(argI(1, GetSizeFlagsVertical()));
        SetSizeFlagsStretchRatio(argF(2, GetSizeFlagsStretchRatio()));
        return nlohmann::json();
    } else if (method == "get_layout_mode") {
        return static_cast<int>(GetLayoutMode());
    } else if (method == "set_layout_mode") {
        int raw = argI(0, static_cast<int>(LayoutMode::Position));
        if (raw < 0) raw = 0;
        if (raw > static_cast<int>(LayoutMode::Anchors)) raw = static_cast<int>(LayoutMode::Anchors);
        SetLayoutMode(static_cast<LayoutMode>(raw));
        return nlohmann::json();

    } else if (method == "grab_focus") {
        GrabFocus();
        return nlohmann::json();
    } else if (method == "release_focus") {
        ReleaseFocus();
        return nlohmann::json();
    } else if (method == "has_focus") {
        return HasFocus();
    } else if (method == "get_focused_control") {
        return controlNodeArg(GetFocusedControl());
    } else if (method == "clear_focus") {
        ClearFocus();
        return nlohmann::json();
    } else if (method == "set_focus_mode") {
        int raw = argI(0, static_cast<int>(FocusMode::All));
        if (raw < 0) raw = 0;
        if (raw > static_cast<int>(FocusMode::All)) raw = static_cast<int>(FocusMode::All);
        SetFocusMode(static_cast<FocusMode>(raw));
        return nlohmann::json();
    } else if (method == "get_focus_mode") {
        return static_cast<int>(GetFocusMode());
    } else if (method == "can_grab_focus") {
        return CanGrabFocus();
    } else if (method == "set_focus_neighbor") {
        SetFocusNeighbor(sideArg(0), argStr(1, std::string()));
        return nlohmann::json();
    } else if (method == "get_focus_neighbor") {
        return GetFocusNeighbor(sideArg(0));
    } else if (method == "set_focus_next") {
        SetFocusNext(argStr(0, std::string()));
        return nlohmann::json();
    } else if (method == "get_focus_next") {
        return GetFocusNext();
    } else if (method == "set_focus_previous") {
        SetFocusPrevious(argStr(0, std::string()));
        return nlohmann::json();
    } else if (method == "get_focus_previous") {
        return GetFocusPrevious();
    } else if (method == "find_valid_focus_neighbor") {
        return controlNodeArg(FindValidFocusNeighbor(sideArg(0)));
    } else if (method == "find_next_valid_focus") {
        return controlNodeArg(FindNextValidFocus());
    } else if (method == "find_prev_valid_focus") {
        return controlNodeArg(FindPrevValidFocus());
    } else if (method == "grab_focus_neighbor") {
        return controlNodeArg(GrabFocusNeighbor(sideArg(0)));
    } else if (method == "grab_focus_next") {
        return controlNodeArg(GrabFocusNext());
    } else if (method == "grab_focus_previous") {
        return controlNodeArg(GrabFocusPrevious());
    } else if (method == "set_mouse_filter") {
        int raw = argI(0, static_cast<int>(MouseFilter::Ignore));
        if (raw < static_cast<int>(MouseFilter::Stop)) raw = static_cast<int>(MouseFilter::Stop);
        if (raw > static_cast<int>(MouseFilter::Ignore)) raw = static_cast<int>(MouseFilter::Ignore);
        SetMouseFilter(static_cast<MouseFilter>(raw));
        return nlohmann::json();
    } else if (method == "get_mouse_filter") {
        return static_cast<int>(GetMouseFilter());
    } else if (method == "is_mouse_hovered") {
        return IsMouseHovered();
    } else if (method == "is_mouse_pressed") {
        int raw = argI(0, static_cast<int>(input::MouseButton::Left));
        if (raw < 0) raw = 0;
        if (raw > static_cast<int>(input::MouseButton::Middle)) raw = static_cast<int>(input::MouseButton::Middle);
        return IsMousePressed(static_cast<input::MouseButton>(raw));
    }
    return nlohmann::json();
}

// ========================================
// Property registration
// ========================================

void UIControl::DefineUIControlProperties(float defaultWidth, float defaultHeight,
                                          const char* uiSpacePropName,
                                          const char* sizeGroup,
                                          bool defaultUISpace) {
    m_UISpacePropName = (uiSpacePropName != nullptr) ? uiSpacePropName : "uiSpace";

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, defaultWidth, 0.0f, 10000.0f, 1.0f, sizeGroup));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, defaultHeight, 0.0f, 10000.0f, 1.0f, sizeGroup));
    DefineProperty(PROPERTY_DEFAULT_GROUP(customMinSize, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(customMaxSize, Vec2, Vec2(0.0f, 0.0f), "Layout"));

    DefineProperty(PROPERTY_ENUM_GROUP(layoutMode, 0, "Layout", Position, Anchors));
    DefineProperty(PROPERTY_ENUM_GROUP(anchorPreset, 0, "Layout",
        TopLeft, TopCenter, TopRight, CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight, LeftWide, TopWide, RightWide,
        BottomWide, VCenterWide, HCenterWide, FullRect, Custom));
    DefineProperty(PROPERTY_DEFAULT_GROUP(anchorMin, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(anchorMax, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offsetMin, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offsetMax, Vec2, Vec2(0.0f, 0.0f), "Layout"));
    DefineProperty(PROPERTY_ENUM_GROUP(growDirectionH, 1, "Layout", Begin, End, Both));
    DefineProperty(PROPERTY_ENUM_GROUP(growDirectionV, 1, "Layout", Begin, End, Both));

    // NOTE: there is deliberately no "pivot" property. It shipped, was exposed through the
    // C API, and the solver never read it -- every control is center-pivoted by
    // WriteRectToNode. Making it real would mean changing what a UI node's POSITION means
    // (the ~200 draw sites across the 2D components all take GetGlobalPosition() to be the
    // rect's center) and threading a rotation-origin through the 2D draw API in every
    // backend. It was removed rather than left as a field that silently does nothing.

    DefineProperty(PROPERTY_INT_RANGE_GROUP(sizeFlagsHorizontal, SizeFlagBits::Fill, 0, 15, 1, "Size Flags"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sizeFlagsVertical, SizeFlagBits::Fill, 0, 15, 1, "Size Flags"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(sizeFlagsStretchRatio, 1.0f, 0.0f, 100.0f, 0.1f, "Size Flags"));

    // The UI-space flag is registered under the caller's historical property name
    // so "uiSpace"/"useUISpace" scenes both keep working.
    DefineProperty(PropertyDescriptor(std::string(m_UISpacePropName),
        PropertyValueType::Bool, PropertyDefaultValue(defaultUISpace),
        PropertyHint(), std::string("Rendering")));

    // Per-subtree theme override + type variation. Empty defaults preserve current
    // behaviour (inherit the project theme / use the base type). The theme-override
    // set is persisted separately in Serialize/Deserialize, not as a property, so it
    // stays out of the inspector.
    DefineProperty(PROPERTY_FILE_GROUP(theme, std::string(""), "*.uitheme", "Theme"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(themeTypeVariation, String, std::string(""), "Theme"));

    // Keyboard / gamepad focus + directional neighbor navigation. focusMode defaults
    // to All so controls participate in traversal out of the box; decorative controls
    // can be set to None. Neighbor / next / previous are NodePath overrides (empty =
    // geometry / tree-order fallback), relative to the owning node.
    DefineProperty(PROPERTY_ENUM_GROUP(focusMode, static_cast<int>(FocusMode::All), "Focus",
        None, Click, All));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusNeighborLeft, NodePath, std::string(""), "Focus"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusNeighborTop, NodePath, std::string(""), "Focus"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusNeighborRight, NodePath, std::string(""), "Focus"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusNeighborBottom, NodePath, std::string(""), "Focus"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusNext, NodePath, std::string(""), "Focus"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(focusPrevious, NodePath, std::string(""), "Focus"));
}

// ========================================
// Size
// ========================================

float UIControl::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void UIControl::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    NotifyLayoutPropertyChanged("width");
}

float UIControl::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void UIControl::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    NotifyLayoutPropertyChanged("height");
}

Vec2 UIControl::GetSize() const {
    return Vec2(GetWidth(), GetHeight());
}

void UIControl::SetSize(const Vec2& size) {
    SetWidth(size.x);
    SetHeight(size.y);
}

Vec2 UIControl::GetCustomMinSize() const {
    return GetPropertyValue<Vec2>("customMinSize");
}

void UIControl::SetCustomMinSize(const Vec2& size) {
    SetPropertyValue<Vec2>("customMinSize", size);
    NotifyLayoutPropertyChanged("customMinSize");
}

Vec2 UIControl::GetCustomMaxSize() const {
    return GetPropertyValue<Vec2>("customMaxSize");
}

void UIControl::SetCustomMaxSize(const Vec2& size) {
    SetPropertyValue<Vec2>("customMaxSize", size);
    NotifyLayoutPropertyChanged("customMaxSize");
}

// ========================================
// Layout mode
// ========================================

LayoutMode UIControl::GetLayoutMode() const {
    return static_cast<LayoutMode>(GetPropertyValue<int>("layoutMode"));
}

void UIControl::SetLayoutMode(LayoutMode mode) {
    SetPropertyValue<int>("layoutMode", static_cast<int>(mode));
    NotifyLayoutPropertyChanged("layoutMode");
}

// ========================================
// Anchors / offsets
// ========================================

Vec2 UIControl::GetAnchorMin() const {
    return GetPropertyValue<Vec2>("anchorMin");
}

void UIControl::SetAnchorMin(const Vec2& anchor) {
    SetPropertyValue<Vec2>("anchorMin", anchor);
    // Switches the control into Anchors mode and marks the preset Custom. Without
    // this, setting an anchor on a default (Position-mode) control did nothing at
    // all: ResolveLayout returns before it ever reads the anchors.
    NotifyLayoutPropertyChanged("anchorMin");
}

Vec2 UIControl::GetAnchorMax() const {
    return GetPropertyValue<Vec2>("anchorMax");
}

void UIControl::SetAnchorMax(const Vec2& anchor) {
    SetPropertyValue<Vec2>("anchorMax", anchor);
    NotifyLayoutPropertyChanged("anchorMax");
}

Vec2 UIControl::GetOffsetMin() const {
    return GetPropertyValue<Vec2>("offsetMin");
}

void UIControl::SetOffsetMin(const Vec2& offset) {
    SetPropertyValue<Vec2>("offsetMin", offset);
    NotifyLayoutPropertyChanged("offsetMin");
}

Vec2 UIControl::GetOffsetMax() const {
    return GetPropertyValue<Vec2>("offsetMax");
}

void UIControl::SetOffsetMax(const Vec2& offset) {
    SetPropertyValue<Vec2>("offsetMax", offset);
    NotifyLayoutPropertyChanged("offsetMax");
}

AnchorPreset UIControl::GetAnchorPreset() const {
    return static_cast<AnchorPreset>(GetPropertyValue<int>("anchorPreset"));
}

void UIControl::SetAnchorPreset(AnchorPreset preset) {
    ApplyAnchorPreset(preset, false);
}

void UIControl::PresetToAnchors(AnchorPreset preset, Vec2& outMin, Vec2& outMax) {
    switch (preset) {
        case AnchorPreset::TopLeft:      outMin = Vec2(0.0f, 0.0f); outMax = Vec2(0.0f, 0.0f); break;
        case AnchorPreset::TopCenter:    outMin = Vec2(0.5f, 0.0f); outMax = Vec2(0.5f, 0.0f); break;
        case AnchorPreset::TopRight:     outMin = Vec2(1.0f, 0.0f); outMax = Vec2(1.0f, 0.0f); break;
        case AnchorPreset::CenterLeft:   outMin = Vec2(0.0f, 0.5f); outMax = Vec2(0.0f, 0.5f); break;
        case AnchorPreset::Center:       outMin = Vec2(0.5f, 0.5f); outMax = Vec2(0.5f, 0.5f); break;
        case AnchorPreset::CenterRight:  outMin = Vec2(1.0f, 0.5f); outMax = Vec2(1.0f, 0.5f); break;
        case AnchorPreset::BottomLeft:   outMin = Vec2(0.0f, 1.0f); outMax = Vec2(0.0f, 1.0f); break;
        case AnchorPreset::BottomCenter: outMin = Vec2(0.5f, 1.0f); outMax = Vec2(0.5f, 1.0f); break;
        case AnchorPreset::BottomRight:  outMin = Vec2(1.0f, 1.0f); outMax = Vec2(1.0f, 1.0f); break;
        case AnchorPreset::LeftWide:     outMin = Vec2(0.0f, 0.0f); outMax = Vec2(0.0f, 1.0f); break;
        case AnchorPreset::TopWide:      outMin = Vec2(0.0f, 0.0f); outMax = Vec2(1.0f, 0.0f); break;
        case AnchorPreset::RightWide:    outMin = Vec2(1.0f, 0.0f); outMax = Vec2(1.0f, 1.0f); break;
        case AnchorPreset::BottomWide:   outMin = Vec2(0.0f, 1.0f); outMax = Vec2(1.0f, 1.0f); break;
        case AnchorPreset::VCenterWide:  outMin = Vec2(0.0f, 0.5f); outMax = Vec2(1.0f, 0.5f); break;
        case AnchorPreset::HCenterWide:  outMin = Vec2(0.5f, 0.0f); outMax = Vec2(0.5f, 1.0f); break;
        case AnchorPreset::FullRect:     outMin = Vec2(0.0f, 0.0f); outMax = Vec2(1.0f, 1.0f); break;
        case AnchorPreset::Custom:
        default:
            outMin = Vec2(0.0f, 0.0f);
            outMax = Vec2(0.0f, 0.0f);
            break;
    }
}

void UIControl::ApplyAnchorPreset(AnchorPreset preset, bool keepOffsets) {
    if (preset == AnchorPreset::Custom) {
        SetPropertyValue<int>("anchorPreset", static_cast<int>(AnchorPreset::Custom));
        SetLayoutMode(LayoutMode::Anchors);
        return;
    }

    Vec2 aMin, aMax;
    PresetToAnchors(preset, aMin, aMax);

    const Vec2 canvas = GetLogicalCanvasSize();
    const Rect parentRect = ResolveParentRect(canvas);

    Vec2 newOffMin(0.0f, 0.0f);
    Vec2 newOffMax(0.0f, 0.0f);

    if (keepOffsets) {
        // Preserve the current on-screen rect: recompute offsets so it does not jump.
        Rect cur = m_ResolvedRect;
        if (cur.size.x <= 0.0f && cur.size.y <= 0.0f) {
            cur = ComputeRectFromNode();
        }
        newOffMin.x = cur.position.x - (parentRect.position.x + aMin.x * parentRect.size.x);
        newOffMax.x = (cur.position.x + cur.size.x) - (parentRect.position.x + aMax.x * parentRect.size.x);
        // Y-up: offsetMin.y is the top-edge offset, offsetMax.y the bottom-edge offset.
        // cur.position.y is the bottom (min-Y) corner; cur.position.y + size.y is the top.
        newOffMin.y = (parentRect.position.y + (1.0f - aMin.y) * parentRect.size.y) - (cur.position.y + cur.size.y);
        newOffMax.y = (parentRect.position.y + (1.0f - aMax.y) * parentRect.size.y) - cur.position.y;
    } else {
        // Snap the control to the preset's natural region using its effective size
        // (max of the requested width/height and the content/min size, so auto-sized
        // controls such as a 0-width Image2D anchor by their real on-screen size).
        Vec2 size = GetSize();
        const Vec2 minSz = GetMinSize();
        size.x = std::max(size.x, minSz.x);
        size.y = std::max(size.y, minSz.y);
        auto naturalOffset = [](float amin, float amax, float sz) -> float {
            if (Equals(amin, amax)) {
                if (amin <= 0.0f) return 0.0f;
                if (amin >= 1.0f) return -sz;
                return -sz * amin;
            }
            return 0.0f;
        };
        newOffMin.x = naturalOffset(aMin.x, aMax.x, size.x);
        newOffMin.y = naturalOffset(aMin.y, aMax.y, size.y);
        newOffMax.x = Equals(aMin.x, aMax.x) ? newOffMin.x : 0.0f;
        newOffMax.y = Equals(aMin.y, aMax.y) ? newOffMin.y : 0.0f;
    }

    SetAnchorMin(aMin);
    SetAnchorMax(aMax);
    SetOffsetMin(newOffMin);
    SetOffsetMax(newOffMax);
    SetPropertyValue<int>("anchorPreset", static_cast<int>(preset));
    SetLayoutMode(LayoutMode::Anchors);
}

// ========================================
// Pivot / grow direction
// ========================================

Vec4 UIControl::GetContentMargins(const std::string& styleboxEntry) const {
    // Start from the themed StyleBox's content margins. Until now nothing anywhere read
    // these -- a theme could set contentMarginLeft = 20 and the label would not move.
    Vec4 margins(0.0f, 0.0f, 0.0f, 0.0f);
    if (std::shared_ptr<StyleBox> styleBox = ResolveThemedStyleBox(styleboxEntry)) {
        margins = Vec4(styleBox->GetContentMarginTop(),
                       styleBox->GetContentMarginRight(),
                       styleBox->GetContentMarginBottom(),
                       styleBox->GetContentMarginLeft());
    }

    // Fold in the control's own padding property, whichever spelling it happens to use.
    Vec4 own(0.0f, 0.0f, 0.0f, 0.0f);

    if (const core::ComponentProperty* textPadding = m_CustomProperties.GetProperty("textPadding")) {
        // Button: Vec2, x = horizontal, y = vertical.
        const Vec2 value = textPadding->GetValue<Vec2>();
        own = Vec4(value.y, value.x, value.y, value.x);
    } else if (const core::ComponentProperty* padding = m_CustomProperties.GetProperty("padding")) {
        // LineEdit/TextEdit/Dropdown/...: a single scalar applied to all four sides.
        // Container also spells it "padding" but as a per-side Vec4, so both are accepted.
        if (padding->GetType() == core::PropertyValueType::Vec4) {
            own = padding->GetValue<Vec4>();
        } else {
            const float value = padding->GetValue<float>();
            own = Vec4(value, value, value, value);
        }
    }

    return Vec4(std::max(margins.x, own.x),
                std::max(margins.y, own.y),
                std::max(margins.z, own.z),
                std::max(margins.w, own.w));
}

Rect UIControl::GetContentInsetRect(const Rect& box, const std::string& styleboxEntry) const {
    const Vec4 margins = GetContentMargins(styleboxEntry);

    // (top, right, bottom, left). FromEdges clamps to zero extent rather than inverting when
    // the insets are larger than the box.
    return Rect::FromEdges(
        box.GetLeftEdge() + margins.w,
        box.GetTopEdge() - margins.x,
        box.GetRightEdge() - margins.y,
        box.GetBottomEdge() + margins.z
    );
}

UIControl::AxisDriver UIControl::GetAxisDriver(bool vertical) const {
    if (m_ContainerDriven) {
        return AxisDriver::ContainerDriven;
    }
    if (GetLayoutMode() == LayoutMode::Position) {
        return AxisDriver::WidthHeight;
    }

    // An axis whose two anchors coincide is point-anchored: ResolveLayout takes its extent
    // from width/height. An axis whose anchors differ is stretched between them, and its
    // extent is (anchor span) + offsets -- width/height are never read for it.
    const Vec2 aMin = GetAnchorMin();
    const Vec2 aMax = GetAnchorMax();
    const bool pointAnchored = vertical ? Equals(aMin.y, aMax.y) : Equals(aMin.x, aMax.x);

    return pointAnchored ? AxisDriver::WidthHeight : AxisDriver::Offsets;
}

bool UIControl::SetAxisExtent(bool vertical, float extent) {
    extent = std::max(0.0f, extent);

    switch (GetAxisDriver(vertical)) {
        case AxisDriver::ContainerDriven:
            // The parent container owns this rect. Refuse, rather than write a property the
            // next layout pass will overwrite.
            return false;

        case AxisDriver::WidthHeight:
            if (vertical) {
                SetHeight(extent);
            } else {
                SetWidth(extent);
            }
            return true;

        case AxisDriver::Offsets: {
            // On a stretched axis the extent is (anchor span across the parent) + offsets.
            // Moving the FAR edge's offset by the difference resizes the control and leaves
            // the near edge (left / top) where it is.
            //
            // Both axes work out the same way despite the Y flip: horizontally,
            // width = (a1 + offsetMax.x) - (a0 + offsetMin.x), so +delta on offsetMax.x
            // widens it; vertically, height = (aTop - offsetMin.y) - (aBot - offsetMax.y),
            // so +delta on offsetMax.y lowers the bottom edge and makes it taller.
            const float current = vertical ? GetRect().size.y : GetRect().size.x;
            const float delta = extent - current;

            Vec2 offsetMax = GetOffsetMax();
            if (vertical) {
                offsetMax.y += delta;
            } else {
                offsetMax.x += delta;
            }
            SetOffsetMax(offsetMax);
            return true;
        }
    }

    return false;
}

bool UIControl::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (is3D) {
        return false;
    }

    // Scale relative to what the user can actually SEE (the resolved, clamped size), not the
    // authored width/height -- on a stretched axis those are not even the same number.
    const Vec2 current = GetBoundsSize();

    bool handled = false;
    if (axis == 0 || axis == -1) {
        handled |= SetAxisExtent(false, std::max(0.1f, current.x + scaleDelta * current.x));
    }
    if (axis == 1 || axis == -1) {
        handled |= SetAxisExtent(true, std::max(0.1f, current.y + scaleDelta * current.y));
    }
    return handled;
}

GrowDirection UIControl::GetGrowDirectionH() const {
    return static_cast<GrowDirection>(GetPropertyValue<int>("growDirectionH"));
}

void UIControl::SetGrowDirectionH(GrowDirection dir) {
    SetPropertyValue<int>("growDirectionH", static_cast<int>(dir));
    NotifyLayoutPropertyChanged("growDirectionH");
}

GrowDirection UIControl::GetGrowDirectionV() const {
    return static_cast<GrowDirection>(GetPropertyValue<int>("growDirectionV"));
}

void UIControl::SetGrowDirectionV(GrowDirection dir) {
    SetPropertyValue<int>("growDirectionV", static_cast<int>(dir));
    NotifyLayoutPropertyChanged("growDirectionV");
}

// ========================================
// Size flags
// ========================================

int UIControl::GetSizeFlagsHorizontal() const {
    return GetPropertyValue<int>("sizeFlagsHorizontal");
}

void UIControl::SetSizeFlagsHorizontal(int flags) {
    SetPropertyValue<int>("sizeFlagsHorizontal", flags);
    // Dirties the parent container: size flags are ITS input, so without this the
    // change did not take effect until something unrelated invalidated the container.
    NotifyLayoutPropertyChanged("sizeFlagsHorizontal");
}

int UIControl::GetSizeFlagsVertical() const {
    return GetPropertyValue<int>("sizeFlagsVertical");
}

void UIControl::SetSizeFlagsVertical(int flags) {
    SetPropertyValue<int>("sizeFlagsVertical", flags);
    NotifyLayoutPropertyChanged("sizeFlagsVertical");
}

float UIControl::GetSizeFlagsStretchRatio() const {
    return GetPropertyValue<float>("sizeFlagsStretchRatio");
}

void UIControl::SetSizeFlagsStretchRatio(float ratio) {
    SetPropertyValue<float>("sizeFlagsStretchRatio", ratio);
    NotifyLayoutPropertyChanged("sizeFlagsStretchRatio");
}

// ========================================
// UI space
// ========================================

bool UIControl::GetUISpace() const {
    return GetPropertyValue<bool>(m_UISpacePropName);
}

void UIControl::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>(m_UISpacePropName, uiSpace);
}

// ========================================
// Minimum size
// ========================================

Vec2 UIControl::GetMinSize() const {
    Vec2 custom = GetCustomMinSize();
    Vec2 content = GetContentMinSize();
    return Vec2(std::max(custom.x, content.x), std::max(custom.y, content.y));
}

Vec2 UIControl::GetMaxSize() const {
    return GetCustomMaxSize();
}

Vec2 UIControl::GetBoundsSize() const {
    Vec2 size = m_ResolvedRect.size;
    if (size.x <= 0.0f && size.y <= 0.0f) {
        // Not resolved yet (e.g. bounds queried before the first layout pass).
        size = GetSize();
    }
    return ClampToMinMax(size);
}

Vec2 UIControl::GetDesiredSize() const {
    // The AUTHORED request, not the arranged rect: measuring must be idempotent, so a
    // container that arranged this control last frame must still see the same desired
    // size this frame. Reading m_ResolvedRect here would reintroduce the growth ratchet.
    return ClampToMinMax(GetSize());
}

Vec2 UIControl::ClampToMinMax(const Vec2& size) const {
    Vec2 minSz = GetMinSize();
    Vec2 result(std::max(size.x, minSz.x), std::max(size.y, minSz.y));
    // Clamp to the custom maximum (0 = unbounded). The minimum always wins, so a
    // max smaller than the min is raised to the min first.
    Vec2 maxSz = GetMaxSize();
    if (maxSz.x > 0.0f) result.x = std::min(result.x, std::max(maxSz.x, minSz.x));
    if (maxSz.y > 0.0f) result.y = std::min(result.y, std::max(maxSz.y, minSz.y));
    return result;
}

// ========================================
// Rect / solver
// ========================================

Rect UIControl::ComputeRectFromNode() const {
    Vec2 size = GetSize();
    Vec2 center(0.0f, 0.0f);
    const Node2D* node2D = dynamic_cast<const Node2D*>(m_Owner);
    if (node2D) {
        center = node2D->GetGlobalPosition();
    }
    return Rect(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y);
}

void UIControl::WriteRectToNode(const Rect& globalRect) {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }
    Vec2 globalCenter = globalRect.GetCenter();
    Vec2 local = globalCenter;
    Node2D* parent2D = dynamic_cast<Node2D*>(m_Owner->GetParent());
    if (parent2D) {
        // Axis-aligned inverse of the parent transform, matching the existing
        // container child-positioning convention.
        local = globalCenter - parent2D->GetGlobalPosition();
    }
    node2D->SetPosition(local);
}

void UIControl::SetRect(const Rect& globalRect) {
    m_ContainerDriven = true;
    m_ResolvedRect = globalRect;
    // NOTE: deliberately does NOT call SetWidth/SetHeight. See the header: width/height
    // are the authored request and feed the next measure pass, so writing the arranged
    // size back would ratchet the control larger every frame and corrupt the saved scene.
    WriteRectToNode(globalRect);
}

Rect UIControl::ResolveParentRect(const Vec2& canvasSize) const {
    Node* p = m_Owner ? m_Owner->GetParent() : nullptr;
    while (p) {
        std::shared_ptr<UIControl> uc = p->GetComponent<UIControl>();
        if (uc) {
            return uc->GetResolvedRect();
        }
        p = p->GetParent();
    }
    // Root canvas rect, centered on the world origin to coincide with the Camera2D
    // default view and the centered CameraUI/CameraCanvas default (origin 0.5,0.5).
    // A top-level FullRect control therefore resolves to a rect centered at (0,0).
    return Rect(-canvasSize.x * 0.5f, -canvasSize.y * 0.5f, canvasSize.x, canvasSize.y);
}

void UIControl::ResolveLayout(const Rect& parentRect, const Vec2& canvasSize) {
    (void)canvasSize;
    if (!m_Owner) {
        return;
    }
    if (m_ContainerDriven) {
        // The parent container owns this control's rect.
        return;
    }

    if (GetLayoutMode() == LayoutMode::Position) {
        // Free placement: the Node2D position drives the rect (legacy behaviour).
        m_ResolvedRect = ComputeRectFromNode();
        return;
    }

    const Vec2 aMin = GetAnchorMin();
    const Vec2 aMax = GetAnchorMax();
    const Vec2 oMin = GetOffsetMin();
    const Vec2 oMax = GetOffsetMax();
    const Vec2 minSize = GetMinSize();
    const Vec2 maxSize = GetMaxSize();

    Rect result;

    // ----- Horizontal axis -----
    {
        float a0 = parentRect.position.x + aMin.x * parentRect.size.x;
        float a1 = parentRect.position.x + aMax.x * parentRect.size.x;
        float left;
        float width;
        if (Equals(aMin.x, aMax.x)) {
            width = GetWidth();
            left = a0 + oMin.x;
        } else {
            left = a0 + oMin.x;
            float right = a1 + oMax.x;
            width = right - left;
        }
        if (width < minSize.x) {
            float deficit = minSize.x - width;
            switch (GetGrowDirectionH()) {
                case GrowDirection::Begin: left -= deficit; break;
                case GrowDirection::Both:  left -= deficit * 0.5f; break;
                case GrowDirection::End:
                default: break;
            }
            width = minSize.x;
        }
        // Clamp to the custom maximum (0 = unbounded), keeping the edge that the grow
        // direction anchors. Begin grows toward/keeps the end (right) edge, so it also
        // shrinks toward it; End keeps the begin (left) edge; Both stays centered. The
        // minimum wins if it exceeds the maximum.
        if (maxSize.x > 0.0f) {
            float effMax = std::max(maxSize.x, minSize.x);
            if (width > effMax) {
                float excess = width - effMax;
                switch (GetGrowDirectionH()) {
                    case GrowDirection::Begin: left += excess; break;
                    case GrowDirection::Both:  left += excess * 0.5f; break;
                    case GrowDirection::End:
                    default: break;
                }
                width = effMax;
            }
        }
        result.position.x = left;
        result.size.x = width;
    }

    // ----- Vertical axis -----
    // The world/2D camera is Y-up (screen top = +Y), so anchor.y = 0 maps to the TOP
    // edge of the parent rect and anchor.y = 1 to the bottom. parentRect.position.y is
    // the bottom (minimum-Y) corner. Offsets are screen-oriented: a positive offset
    // moves the corresponding edge downward (toward -Y), matching the X axis (right).
    {
        float aTop = parentRect.position.y + (1.0f - aMin.y) * parentRect.size.y;
        float aBot = parentRect.position.y + (1.0f - aMax.y) * parentRect.size.y;
        float topEdge;
        float botEdge;
        float height;
        if (Equals(aMin.y, aMax.y)) {
            height = GetHeight();
            topEdge = aTop - oMin.y;
            botEdge = topEdge - height;
        } else {
            topEdge = aTop - oMin.y;
            botEdge = aBot - oMax.y;
            height = topEdge - botEdge;
        }
        if (height < minSize.y) {
            float deficit = minSize.y - height;
            switch (GetGrowDirectionV()) {
                case GrowDirection::Begin: break;                       // keep bottom edge, grow upward
                case GrowDirection::Both:  botEdge -= deficit * 0.5f; break;
                case GrowDirection::End:
                default: botEdge -= deficit; break;                     // keep top edge, grow downward
            }
            height = minSize.y;
        }
        // Clamp to the custom maximum (0 = unbounded). Mirror the grow direction used
        // when growing: Begin keeps the bottom edge, End keeps the top edge, Both stays
        // centered. The minimum wins if it exceeds the maximum.
        if (maxSize.y > 0.0f) {
            float effMax = std::max(maxSize.y, minSize.y);
            if (height > effMax) {
                float excess = height - effMax;
                switch (GetGrowDirectionV()) {
                    case GrowDirection::Begin: break;
                    case GrowDirection::Both:  botEdge += excess * 0.5f; break;
                    case GrowDirection::End:
                    default: botEdge += excess; break;
                }
                height = effMax;
            }
        }
        result.position.y = botEdge;
        result.size.y = height;
    }

    m_ResolvedRect = result;

    // NOTE: do NOT write the resolved size back into width/height. width/height are the
    // control's REQUESTED size and drive point-anchored axes; the actual on-screen size
    // (which may be stretched by anchors) lives in m_ResolvedRect / GetRect(). Writing the
    // stretched size back would bloat width/height and corrupt the next preset switch
    // (e.g. Top Wide -> Right Wide would inherit full width as the "natural" width).

    WriteRectToNode(result);
}

void UIControl::ResolveSelf() {
    if (!m_Owner) {
        return;
    }

    const Vec2 canvas = GetLogicalCanvasSize();

    // If the immediate parent is a layout container, it owns this control's rect and
    // has already assigned it through SetRect() during its own arrange pass (which runs
    // parent-before-child). Leave m_ResolvedRect untouched: recomputing it from the node
    // transform plus the width/height properties would discard the arranged size and
    // snap the control back to its authored request.
    //
    // Before the container's first arrange the resolved rect is still empty; fall back to
    // the node-derived rect so bounds/hit-testing have something sane to work with.
    Node* parent = m_Owner->GetParent();
    if (parent) {
        std::shared_ptr<UIControl> parentUC = parent->GetComponent<UIControl>();
        if (parentUC && parentUC->IsLayoutContainer()) {
            m_ContainerDriven = true;
            if (m_ResolvedRect.size.x <= 0.0f && m_ResolvedRect.size.y <= 0.0f) {
                m_ResolvedRect = ComputeRectFromNode();
            }
            return;
        }
    }
    m_ContainerDriven = false;

    const Rect parentRect = ResolveParentRect(canvas);
    ResolveLayout(parentRect, canvas);
}

// ========================================
// Lifecycle
// ========================================

void UIControl::PerformLayout() {
    ResolveSelf();
}

void UIControl::LayoutPass(core::Node* root) {
    if (!root) {
        return;
    }

    // Pre-order: this node's controls first, then descend. A child's anchors resolve
    // against ResolveParentRect(), so the parent's rect must already be final.
    for (const std::shared_ptr<Component>& comp : root->GetComponents()) {
        UIControl* control = dynamic_cast<UIControl*>(comp.get());
        if (control && control->IsEnabled()) {
            control->PerformLayout();
        }
    }

    for (const std::shared_ptr<Node>& child : root->GetChildren()) {
        LayoutPass(child.get());
    }
}

void UIControl::OnUpdate(float deltaTime) {
    (void)deltaTime;
    PerformLayout();
}

void UIControl::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // Editing a themeable property in the inspector makes it a local override so the
    // explicit value wins over the theme (matches Godot's add_theme_*_override).
    if (IsThemeableProperty(propertyName)) {
        SetThemeOverride(propertyName, true);
    }

    // The theme path drives the effective theme of this control and its descendants.
    // The editor edits it through this path (not SetThemePath), so signal it here so
    // every dependent effective-theme cache recomputes.
    if (propertyName == "theme") {
        ++s_ThemeOwnerGeneration;
        return;
    }

    if (propertyName == "anchorPreset") {
        int idx = newValue.is_number_integer() ? newValue.get<int>()
                : (newValue.is_number() ? static_cast<int>(newValue.get<double>()) : 0);
        if (idx < 0) {
            // Clicking the active preset again disables anchoring: revert to free
            // (Position) layout so the control follows its Node2D position again.
            SetLayoutMode(LayoutMode::Position);
            InvalidateParentContainerLayout();
        } else if (idx != static_cast<int>(AnchorPreset::Custom)) {
            ApplyAnchorPreset(static_cast<AnchorPreset>(idx), false);
        }
        return;
    }

    if (propertyName == "anchorMin" || propertyName == "anchorMax") {
        // Direct anchor edits switch to a custom preset in Anchors mode.
        SetPropertyValue<int>("anchorPreset", static_cast<int>(AnchorPreset::Custom));
        SetLayoutMode(LayoutMode::Anchors);
        InvalidateParentContainerLayout();
        return;
    }

    if (propertyName == "layoutMode") {
        // Switching to Position (free) layout clears any active preset so the anchor
        // picker shows nothing selected, regardless of how the mode was changed.
        int mode = newValue.is_number() ? static_cast<int>(newValue.get<double>()) : 0;
        if (mode == static_cast<int>(LayoutMode::Position)) {
            SetPropertyValue<int>("anchorPreset", -1);
        }
        InvalidateParentContainerLayout();
        return;
    }

    if (propertyName == "offsetMin" || propertyName == "offsetMax" ||
        propertyName == "width" || propertyName == "height" ||
        propertyName == "customMinSize" || propertyName == "customMaxSize" ||
        propertyName == "sizeFlagsHorizontal" || propertyName == "sizeFlagsVertical" ||
        propertyName == "sizeFlagsStretchRatio" ||
        // ...and anything that changes what this control MEASURES, which a parent
        // container's arrangement depends on just as much as the layout properties do.
        IsContentSizeProperty(propertyName)) {
        InvalidateParentContainerLayout();
    }
}

void UIControl::NotifyLayoutPropertyChanged(const std::string& propertyName) {
    const core::ComponentProperty* prop = m_CustomProperties.GetProperty(propertyName);
    if (!prop) {
        return;
    }
    // Virtual dispatch is deliberate: a Container's override must also re-run its own
    // layout, so SetWidth() on a container behaves the same whether it came from the
    // inspector, a script, or the C API.
    OnPropertyChanged(propertyName, prop->GetValueAsJson());
}

bool UIControl::IsContentSizeProperty(const std::string& propertyName) const {
    // The union of every property, across the whole control family, that can change what a
    // control reports from GetContentMinSize(). Grouped by what defines them.
    static const std::unordered_set<std::string> kContentProperties = {
        // Text controls: the string itself, the font it is measured with, and everything
        // that changes how it is broken into lines or padded.
        "text", "placeholder", "placeholderText",
        "fontPath", "fontSize",
        "wordWrap", "autowrapMode", "overrunBehavior", "clipText",
        "lineSpacing", "letterSpacing", "outlineWidth",
        "textPadding", "scaleMode",

        // List/tree/menu controls: row height and item count drive their minimum.
        "items", "itemHeight",

        // Check/radio/toggle: the indicator box sits beside the label.
        "boxSize", "spacing",

        // Slider: the handle and track define the control's thickness.
        "handleRadius", "trackThickness", "orientation",

        // Image/nine-slice: the source texture supplies the intrinsic size, and the slice
        // margins are a hard floor on it.
        "texturePath", "texture", "margin", "marginLinked",

        // Containers: their minimum is their children plus their own spacing and padding,
        // so these change the size the container reports to ITS parent.
        "padding", "paddingLinked", "separation",
        "horizontalSizeMode", "verticalSizeMode",
        "horizontalSpacing", "verticalSpacing", "spacingX", "spacingY", "dockSpacing",
        "rowCount", "columnCount", "useRows",
        "cellSizingMode", "fixedCellWidth", "fixedCellHeight", "homogeneousCells",
        "maxLines", "wrapDirection",
        "tabHeight", "scrollBarWidth",
    };

    return kContentProperties.count(propertyName) != 0;
}

void UIControl::InvalidateParentContainerLayout() {
    if (!m_Owner) {
        return;
    }

    // Walk up to the nearest LAYOUT CONTAINER ancestor, not merely the immediate parent.
    // A control nested under a plain grouping node (or a non-container UIControl such as a
    // Panel used purely as a backdrop) still has its arrangement decided by the container
    // above that, and stopping at the first parent left it never re-laid-out.
    for (Node* ancestor = m_Owner->GetParent(); ancestor; ancestor = ancestor->GetParent()) {
        std::shared_ptr<UIControl> uc = ancestor->GetComponent<UIControl>();
        if (uc && uc->IsLayoutContainer()) {
            uc->OnChildLayoutChanged();
            return;
        }
    }
}

SpatialType UIControl::GetUISpatialType() const {
    return GetUISpace() ? SpatialType::Canvas : SpatialType::World2D;
}

math::Vec2 UIControl::GetCanvasMousePosition() const {
    input::InputManager& inputMgr = input::InputManager::Get();
    glm::vec2 rawMousePos = inputMgr.GetMousePosition();

    Viewport viewport = GetCurrentViewport();
    math::Vec2 logicalSize = GetLogicalCanvasSize();

    float nx = (viewport.width > 0.0f) ? (rawMousePos.x - viewport.x) / viewport.width : 0.0f;
    float ny = (viewport.height > 0.0f) ? (rawMousePos.y - viewport.y) / viewport.height : 0.0f;

    // Preferred path: the runtime publishes the inverse of the exact view-projection it
    // drew the UI with, so we map the cursor's NDC straight into control space. This
    // correctly undoes the active CameraUI's origin, scale_factor, pixel snapping, zoom,
    // rotation, position, shake and follow in a single transform.
    math::Mat4 invViewProj;
    if (GetCanvasPickMatrix(invViewProj)) {
        float ndcX = nx * 2.0f - 1.0f;
        float ndcY = 1.0f - ny * 2.0f;
        math::Vec4 control = invViewProj * math::Vec4(ndcX, ndcY, 0.0f, 1.0f);
        if (!math::IsZero(control.w)) {
            control = control / control.w;
        }
        return math::Vec2(control.x, control.y);
    }

    // Fallback (e.g. in the editor, before any runtime frame publishes a pick matrix):
    // assume a centered, Y-up canvas. Control rects live in the centered canvas space
    // (ResolveParentRect's root rect is Rect(-W/2,-H/2,W,H)); screen-top (ny=0) -> +H/2,
    // screen-left (nx=0) -> -W/2.
    return math::Vec2((nx - 0.5f) * logicalSize.x, (0.5f - ny) * logicalSize.y);
}

// ========================================
// Theme
// ========================================

std::string UIControl::GetThemeTypeVariation() const {
    return GetPropertyValue<std::string>("themeTypeVariation");
}

void UIControl::SetThemeTypeVariation(const std::string& variation) {
    SetPropertyValue<std::string>("themeTypeVariation", variation);
}

std::string UIControl::GetThemePath() const {
    return GetPropertyValue<std::string>("theme");
}

void UIControl::SetThemePath(const std::string& resPath) {
    SetPropertyValue<std::string>("theme", resPath);
    // The theme path drives this control's (and its descendants') effective theme;
    // signal it so the cached resolutions recompute. SetPropertyValue does not route
    // through OnPropertyChanged, so the editor path is bumped separately there.
    ++s_ThemeOwnerGeneration;
}

const ui::ThemeAsset* UIControl::GetEffectiveTheme() const {
    ui::ThemeManager& mgr = ui::ThemeManager::GetInstance();
    const uint64_t treeVer  = core::Node::GetTreeStructureVersion();
    const uint64_t ownerGen = s_ThemeOwnerGeneration;
    const uint64_t mgrVer   = mgr.Version();

    // Cache hit: none of the three inputs the resolution depends on has changed.
    if (m_EffectiveThemeCacheValid
        && treeVer  == m_CachedTreeStructVersion
        && ownerGen == m_CachedThemeOwnerGen
        && mgrVer   == m_CachedThemeMgrVersion) {
        return m_CachedEffectiveTheme;
    }

    // This control's own theme override, else the nearest ancestor UIControl that
    // sets one.
    std::string themePath = GetThemePath();
    if (themePath.empty() && m_Owner) {
        Node* p = m_Owner->GetParent();
        while (p) {
            std::shared_ptr<UIControl> uc = p->GetComponent<UIControl>();
            if (uc) {
                std::string t = uc->GetThemePath();
                if (!t.empty()) {
                    themePath = t;
                    break;
                }
            }
            p = p->GetParent();
        }
    }

    const ui::ThemeAsset* resolved = nullptr;
    if (!themePath.empty()) {
        asset::AssetRef<ui::ThemeAsset> theme = mgr.GetOrLoadTheme(themePath);
        if (theme) {
            resolved = theme.Get();
        }
    }
    if (!resolved) {
        resolved = mgr.GetActiveTheme();
    }

    m_CachedEffectiveTheme     = resolved;
    m_CachedTreeStructVersion  = treeVer;
    m_CachedThemeOwnerGen      = ownerGen;
    m_CachedThemeMgrVersion    = mgrVer;
    m_EffectiveThemeCacheValid = true;
    return resolved;
}

const std::vector<UIControl::ThemeBinding>& UIControl::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kEmpty;
    return kEmpty;
}

bool UIControl::IsThemeableProperty(const std::string& property) const {
    for (const ThemeBinding& b : GetThemeBindings()) {
        if (b.property == property) {
            return true;
        }
    }
    return false;
}

bool UIControl::IsThemeOverridden(const std::string& property) const {
    return m_ThemeOverrides.find(property) != m_ThemeOverrides.end();
}

void UIControl::SetThemeOverride(const std::string& property, bool overridden) {
    if (overridden) {
        m_ThemeOverrides.insert(property);
    } else {
        m_ThemeOverrides.erase(property);
    }
}

void UIControl::ClearThemeOverrides() {
    m_ThemeOverrides.clear();
}

math::Color UIControl::ResolveThemedColor(const std::string& property, const std::string& entry) const {
    math::Color own = GetPropertyValue<math::Color>(property);
    if (IsThemeOverridden(property)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    return ui::ThemeManager::GetInstance().ResolveColor(
        theme, GetThemeTypeName(), GetThemeTypeVariation(), entry, own);
}

float UIControl::ResolveThemedConstant(const std::string& property, const std::string& entry) const {
    float own = GetPropertyValue<float>(property);
    if (IsThemeOverridden(property)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    return ui::ThemeManager::GetInstance().ResolveConstant(
        theme, GetThemeTypeName(), GetThemeTypeVariation(), entry, own);
}

math::Vec2 UIControl::ResolveThemedVec2(const std::string& property, const std::string& entry) const {
    math::Vec2 own = GetPropertyValue<math::Vec2>(property);
    if (IsThemeOverridden(property)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    return ui::ThemeManager::GetInstance().ResolveVec2(
        theme, GetThemeTypeName(), GetThemeTypeVariation(), entry, own);
}

bool UIControl::ResolveThemedBool(const std::string& property, const std::string& entry) const {
    bool own = GetPropertyValue<bool>(property);
    if (IsThemeOverridden(property)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    return ui::ThemeManager::GetInstance().ResolveBool(
        theme, GetThemeTypeName(), GetThemeTypeVariation(), entry, own);
}

std::string UIControl::ResolveThemedFontPath(const std::string& pathProperty,
                                             const std::string& fontEntry) const {
    std::string own = GetPropertyValue<std::string>(pathProperty);
    if (IsThemeOverridden(pathProperty)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
    std::string fontPath;
    float fontSize = 0.0f;
    if (tm.ResolveFont(theme, GetThemeTypeName(), GetThemeTypeVariation(), fontEntry, fontPath, fontSize)) {
        if (!fontPath.empty()) {
            return fontPath;
        }
    }
    // Fall back to the theme-wide default font (Godot Theme.default_font parity).
    std::string defPath;
    float defSize = 0.0f;
    if (tm.GetDefaultFont(theme, defPath, defSize) && !defPath.empty()) {
        return defPath;
    }
    return own;
}

std::string UIControl::ResolveThemedImage(const std::string& pathProperty,
                                          const std::string& imageEntry) const {
    std::string own = GetPropertyValue<std::string>(pathProperty);
    if (IsThemeOverridden(pathProperty)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    std::string path;
    if (ui::ThemeManager::GetInstance().ResolveImage(
            theme, GetThemeTypeName(), GetThemeTypeVariation(), imageEntry, path)) {
        if (!path.empty()) {
            return path;
        }
    }
    return own;
}

bool UIControl::ResolveThemedImageEx(const std::string& pathProperty, const std::string& imageEntry,
                                     ui::ThemeImage& out) const {
    if (IsThemeOverridden(pathProperty)) {
        return false;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    ui::ThemeImage resolved;
    if (!ui::ThemeManager::GetInstance().ResolveImageEx(
            theme, GetThemeTypeName(), GetThemeTypeVariation(), imageEntry, resolved)) {
        return false;
    }
    if (resolved.path.empty()) {
        return false;
    }
    out = resolved;
    return true;
}

float UIControl::ResolveThemedFontSize(const std::string& sizeProperty, const std::string& sizeEntry,
                                       const std::string& fontEntry) const {
    float own = GetPropertyValue<float>(sizeProperty);
    if (IsThemeOverridden(sizeProperty)) {
        return own;
    }
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
    float baseScale = tm.GetDefaultBaseScale(theme);
    // An explicit 'font_size' constant wins over a font role's bundled size.
    if (tm.HasConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), sizeEntry)) {
        return tm.ResolveConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), sizeEntry, own) * baseScale;
    }
    std::string fontPath;
    float roleSize = own;
    if (tm.ResolveFont(theme, GetThemeTypeName(), GetThemeTypeVariation(), fontEntry, fontPath, roleSize)) {
        return roleSize * baseScale;
    }
    // Fall back to the theme-wide default font size (Godot Theme.default_font_size).
    std::string defPath;
    float defSize = 0.0f;
    if (tm.GetDefaultFont(theme, defPath, defSize) && defSize > 0.0f) {
        return defSize * baseScale;
    }
    return own;
}

std::shared_ptr<StyleBox> UIControl::ResolveThemedStyleBox(const std::string& styleboxEntry) const {
    const ui::ThemeAsset* theme = GetEffectiveTheme();
    ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
    uint64_t version = tm.Version();
    if (theme != m_ThemedStyleBoxCacheTheme || version != m_ThemedStyleBoxCacheVersion) {
        m_ThemedStyleBoxCache.clear();
        m_ThemedStyleBoxCacheTheme = theme;
        m_ThemedStyleBoxCacheVersion = version;
    }
    std::unordered_map<std::string, std::shared_ptr<StyleBox>>::iterator it =
        m_ThemedStyleBoxCache.find(styleboxEntry);
    if (it != m_ThemedStyleBoxCache.end()) {
        return it->second;
    }
    std::shared_ptr<StyleBox> box = tm.ResolveStyleBox(
        theme, GetThemeTypeName(), GetThemeTypeVariation(), styleboxEntry);
    m_ThemedStyleBoxCache[styleboxEntry] = box;
    return box;
}

bool UIControl::ConsumeThemeVersionChanged() {
    uint64_t version = ui::ThemeManager::GetInstance().Version();
    if (version != m_ThemeVersionSeen) {
        m_ThemeVersionSeen = version;
        return true;
    }
    return false;
}

nlohmann::json UIControl::Serialize() const {
    nlohmann::json json = core::Component::Serialize();
    // Persist the theme-override set alongside the properties (not as a registered
    // property, so it stays out of the inspector).
    if (json.contains("properties") && json["properties"].is_object()) {
        nlohmann::json overrides = nlohmann::json::array();
        for (const std::string& prop : m_ThemeOverrides) {
            overrides.push_back(prop);
        }
        json["properties"]["_themeOverrides"] = overrides;
    }
    return json;
}

void UIControl::Deserialize(const nlohmann::json& json) {
    core::Component::Deserialize(json);

    // A themeable property is a local override only when its serialized value
    // differs from the component's registered default. This is applied uniformly —
    // for legacy (pre-theme) scenes, theme-aware scenes, AND scenes whose
    // "_themeOverrides" marker was written by an earlier build — so default-valued
    // controls are always theme-driven, while any colour/size a scene explicitly
    // customized is preserved (zero regression). The marker is intentionally NOT
    // trusted here: an older build could have written one listing default-valued
    // props, which would otherwise pin them as overrides forever. (Freshly
    // constructed controls keep an empty set and are fully theme-driven; runtime
    // edits add overrides via OnPropertyChanged / SetThemedProperty.)
    m_ThemeOverrides.clear();
    for (const ThemeBinding& b : GetThemeBindings()) {
        const core::ComponentProperty* prop = m_CustomProperties.GetProperty(b.property);
        if (prop && prop->GetValueAsJson() != prop->GetDescriptor().GetDefaultAsJson()) {
            m_ThemeOverrides.insert(b.property);
        }
    }
}

} // namespace components
} // namespace lupine
