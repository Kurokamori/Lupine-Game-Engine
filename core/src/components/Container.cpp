#include "lupine/components/Container.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;

Container::Container()
    : UIControl("Container")
    , m_LayoutDirty(true)
    , m_CachedSize(100.0f, 100.0f)
    , m_CachedPosition(0.0f, 0.0f)
    , m_CachedChildCount(0)
    , m_CachedVisibleChildCount(0)
    , m_Padding(0.0f, true)
    , m_Margin(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_CornerRadius(0.0f, true)
    , m_StyleBox(nullptr)
    , m_HorizontalSizeMode(SizeMode::Fixed)
    , m_VerticalSizeMode(SizeMode::Fixed)
    , m_ClipChildren(false)
    , m_Separation(0.0f)
    , m_MeshNeedsRegeneration(true)
{
    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Container::Container(const std::string& name)
    : UIControl(name)
    , m_LayoutDirty(true)
    , m_CachedSize(100.0f, 100.0f)
    , m_CachedPosition(0.0f, 0.0f)
    , m_CachedChildCount(0)
    , m_CachedVisibleChildCount(0)
    , m_Padding(0.0f, true)
    , m_Margin(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_CornerRadius(0.0f, true)
    , m_StyleBox(nullptr)
    , m_HorizontalSizeMode(SizeMode::Fixed)
    , m_VerticalSizeMode(SizeMode::Fixed)
    , m_ClipChildren(false)
    , m_Separation(0.0f)
    , m_MeshNeedsRegeneration(true)
{
    m_StyleBox = std::make_shared<StyleBoxFlat>();
}

Container::~Container() {
}

void Container::DefineProperties() {
    // Shared UIControl layout properties (width/height/anchors/size-flags/useUISpace).
    DefineUIControlProperties(100.0f, 100.0f, "useUISpace", "Size");

    // Size properties. There is deliberately no minSize/maxSize pair here: those used to
    // shadow UIControl's customMinSize/customMaxSize, giving the inspector two different
    // fields with the same name and meaning of which only customMin/MaxSize were read by
    // the anchor solver. Constraints now live solely on UIControl.
    DefineProperty(PROPERTY_ENUM_GROUP(horizontalSizeMode, 0, "Size", Fixed, FitChildren, Expand, Minimum));
    DefineProperty(PROPERTY_ENUM_GROUP(verticalSizeMode, 0, "Size", Fixed, FitChildren, Expand, Minimum));

    // Padding properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(paddingLinked, Bool, true, "Padding"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(padding, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Padding"));

    // Margin properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(marginLinked, Bool, true, "Margin"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(margin, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "Margin"));

    // Background properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(drawBackground, Bool, true, "Background"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    // Border properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, false, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(1.0f, 1.0f, 1.0f, 1.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.5f, 0.5f, 0.5f, 1.0f), "Border"));

    // Corner radius properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.0f, 0.0f, 0.0f, 0.0f), "CornerRadius"));

    // Layout properties
    DefineProperty(PROPERTY_DEFAULT_GROUP(clipChildren, Bool, false, "Layout"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(separation, 0.0f, 0.0f, 100.0f, 1.0f, "Layout"));

    // Rendering properties
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
}

void Container::OnAwake() {
    // Initialize padding
    Vec4 paddingVec = GetPropertyValue<Vec4>("padding");
    bool paddingLinked = GetPropertyValue<bool>("paddingLinked");
    m_Padding.FromVec4(paddingVec);
    m_Padding.SetLinked(paddingLinked);

    // Initialize margin
    Vec4 marginVec = GetPropertyValue<Vec4>("margin");
    bool marginLinked = GetPropertyValue<bool>("marginLinked");
    m_Margin.FromVec4(marginVec);
    m_Margin.SetLinked(marginLinked);

    // Initialize border width
    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    // Initialize corner radius
    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    // Initialize size modes
    m_HorizontalSizeMode = static_cast<SizeMode>(GetPropertyValue<int>("horizontalSizeMode"));
    m_VerticalSizeMode = static_cast<SizeMode>(GetPropertyValue<int>("verticalSizeMode"));

    // Initialize layout properties
    m_ClipChildren = GetPropertyValue<bool>("clipChildren");
    m_Separation = GetPropertyValue<float>("separation");

    m_LayoutDirty = true;
}

void Container::OnReady() {
    m_LayoutDirty = true;
    m_MeshNeedsRegeneration = true;
}

void Container::OnUpdate(float) {
    PerformLayout();
}

void Container::PerformLayout() {
    if (!IsEnabled()) {
        return;
    }

    // Resolve this container's own rect (anchors/offsets, or container-driven by a
    // parent container) before laying out children against GetContentRect().
    ResolveSelf();

    SyncFromProperties();

    // Detect child changes (additions, removals, reordering, reparenting, visibility) so
    // the layout updates when the scene tree is edited. Comparing the child LIST, not
    // just its length: the old code compared only counts, so swapping two children of a
    // VBox -- or replacing one with another -- changed nothing and left them drawn in
    // their previous slots.
    if (m_Owner) {
        m_CachedChildCount = m_Owner->GetChildCount();

        std::vector<Node*> visibleChildren = GetVisibleChildren();
        if (visibleChildren != m_CachedVisibleChildren) {
            m_CachedVisibleChildren = visibleChildren;
            m_CachedVisibleChildCount = visibleChildren.size();
            InvalidateLayout();
        }
    }

    // Recalculate layout if dirty. RunLayout() re-runs to a fixed point, so an
    // invalidation raised from inside CalculateLayout() is not swallowed.
    if (m_LayoutDirty) {
        RunLayout();
    }
}

void Container::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // Let the base handle anchor-preset / layout-mode side effects.
    UIControl::OnPropertyChanged(propertyName, newValue);

    // Only re-run layout for properties that can actually change the arrangement.
    // This used to invalidate on ANY property, including backgroundColor and opacity, so
    // a colour tween re-laid-out the entire subtree every frame.
    if (!IsPurelyVisualProperty(propertyName)) {
        InvalidateLayout();
    }

    // The cached enums below are otherwise only refreshed in OnAwake(), which the editor
    // never calls -- so a scene reopened in the editor laid out with constructor defaults
    // until the property was nudged. SyncFromProperties() re-reads them every update; this
    // just makes the response to an inspector edit immediate.
    SyncFromProperties();
}

bool Container::IsPurelyVisualProperty(const std::string& propertyName) {
    // Properties that only affect how the container paints itself -- never the size or
    // position of anything, including its own rect.
    return propertyName == "backgroundColor" ||
           propertyName == "borderColor" ||
           propertyName == "opacity" ||
           propertyName == "drawBackground" ||
           propertyName == "layer" ||
           propertyName == "sortingOrder";
}

bool Container::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // A container whose size mode is not Fixed derives its size from its children or its
    // parent, and GetWidth()/GetHeight() ignore the width/height properties entirely in
    // those modes -- so a resize has nowhere to go. Refuse it rather than accept the drag
    // and silently discard it.
    if (m_HorizontalSizeMode != SizeMode::Fixed && m_VerticalSizeMode != SizeMode::Fixed) {
        return false;
    }

    // The base writes whichever property actually drives the axis (width/height, offsets, or
    // nothing at all when this container is itself driven by an outer one).
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    if (handled) {
        InvalidateLayout();
        m_MeshNeedsRegeneration = true;
    }
    return handled;
}

// ========================================
// Size Management
// ========================================

float Container::GetWidth() const {
    if (m_HorizontalSizeMode != SizeMode::Fixed) {
        return CalculateFinalSize().x;
    }
    return GetPropertyValue<float>("width");
}

void Container::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    // Goes through OnPropertyChanged, so it invalidates BOTH our own layout and the parent
    // container's -- our width is an input to the parent's arrangement. A bare
    // InvalidateLayout() only did the former.
    NotifyLayoutPropertyChanged("width");
}

float Container::GetHeight() const {
    if (m_VerticalSizeMode != SizeMode::Fixed) {
        return CalculateFinalSize().y;
    }
    return GetPropertyValue<float>("height");
}

void Container::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    NotifyLayoutPropertyChanged("height");
}

Vec2 Container::GetSize() const {
    return Vec2(GetWidth(), GetHeight());
}

void Container::SetSize(const Vec2& size) {
    SetWidth(size.x);
    SetHeight(size.y);
}

Container::SizeMode Container::GetHorizontalSizeMode() const {
    return m_HorizontalSizeMode;
}

void Container::SetHorizontalSizeMode(SizeMode mode) {
    m_HorizontalSizeMode = mode;
    SetPropertyValue<int>("horizontalSizeMode", static_cast<int>(mode));
    InvalidateLayout();
}

Container::SizeMode Container::GetVerticalSizeMode() const {
    return m_VerticalSizeMode;
}

void Container::SetVerticalSizeMode(SizeMode mode) {
    m_VerticalSizeMode = mode;
    SetPropertyValue<int>("verticalSizeMode", static_cast<int>(mode));
    InvalidateLayout();
}

// ========================================
// Padding & Margin
// ========================================

bool Container::GetPaddingLinked() const {
    return m_Padding.IsLinked();
}

void Container::SetPaddingLinked(bool linked) {
    m_Padding.SetLinked(linked);
    SetPropertyValue<bool>("paddingLinked", linked);
    InvalidateLayout();
}

float Container::GetPaddingLeft() const {
    return m_Padding.Get(3);
}

void Container::SetPaddingLeft(float padding) {
    m_Padding.Set(3, padding);
    SetPropertyValue<Vec4>("padding", m_Padding.AsVec4());
    InvalidateLayout();
}

float Container::GetPaddingRight() const {
    return m_Padding.Get(1);
}

void Container::SetPaddingRight(float padding) {
    m_Padding.Set(1, padding);
    SetPropertyValue<Vec4>("padding", m_Padding.AsVec4());
    InvalidateLayout();
}

float Container::GetPaddingTop() const {
    return m_Padding.Get(0);
}

void Container::SetPaddingTop(float padding) {
    m_Padding.Set(0, padding);
    SetPropertyValue<Vec4>("padding", m_Padding.AsVec4());
    InvalidateLayout();
}

float Container::GetPaddingBottom() const {
    return m_Padding.Get(2);
}

void Container::SetPaddingBottom(float padding) {
    m_Padding.Set(2, padding);
    SetPropertyValue<Vec4>("padding", m_Padding.AsVec4());
    InvalidateLayout();
}

Vec4 Container::GetPadding() const {
    return m_Padding.AsVec4();
}

void Container::SetPadding(const Vec4& padding) {
    m_Padding.FromVec4(padding);
    SetPropertyValue<Vec4>("padding", padding);
    InvalidateLayout();
}

bool Container::GetMarginLinked() const {
    return m_Margin.IsLinked();
}

void Container::SetMarginLinked(bool linked) {
    m_Margin.SetLinked(linked);
    SetPropertyValue<bool>("marginLinked", linked);
    InvalidateLayout();
}

float Container::GetMarginLeft() const {
    return m_Margin.Get(3);
}

void Container::SetMarginLeft(float margin) {
    m_Margin.Set(3, margin);
    SetPropertyValue<Vec4>("margin", m_Margin.AsVec4());
    InvalidateLayout();
}

float Container::GetMarginRight() const {
    return m_Margin.Get(1);
}

void Container::SetMarginRight(float margin) {
    m_Margin.Set(1, margin);
    SetPropertyValue<Vec4>("margin", m_Margin.AsVec4());
    InvalidateLayout();
}

float Container::GetMarginTop() const {
    return m_Margin.Get(0);
}

void Container::SetMarginTop(float margin) {
    m_Margin.Set(0, margin);
    SetPropertyValue<Vec4>("margin", m_Margin.AsVec4());
    InvalidateLayout();
}

float Container::GetMarginBottom() const {
    return m_Margin.Get(2);
}

void Container::SetMarginBottom(float margin) {
    m_Margin.Set(2, margin);
    SetPropertyValue<Vec4>("margin", m_Margin.AsVec4());
    InvalidateLayout();
}

Vec4 Container::GetMargin() const {
    return m_Margin.AsVec4();
}

void Container::SetMargin(const Vec4& margin) {
    m_Margin.FromVec4(margin);
    SetPropertyValue<Vec4>("margin", margin);
    InvalidateLayout();
}

// ========================================
// Visual Appearance
// ========================================

const std::vector<UIControl::ThemeBinding>& Container::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "backgroundColor", "background",    ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",  ThemeBinding::Kind::Color },
        { "cornerRadius",    "corner_radius", ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Container::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void Container::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
    if (m_StyleBox) {
        m_StyleBox->SetBackgroundColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Container::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void Container::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
    if (m_StyleBox) {
        m_StyleBox->SetOpacity(opacity);
    }
    m_MeshNeedsRegeneration = true;
}

bool Container::GetDrawBackground() const {
    return GetPropertyValue<bool>("drawBackground");
}

void Container::SetDrawBackground(bool draw) {
    SetPropertyValue<bool>("drawBackground", draw);
    m_MeshNeedsRegeneration = true;
}

bool Container::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Container::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
    m_MeshNeedsRegeneration = true;
}

bool Container::GetBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Container::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
    m_MeshNeedsRegeneration = true;
}

float Container::GetBorderWidthLeft() const {
    return m_BorderWidth.Get(3);
}

void Container::SetBorderWidthLeft(float width) {
    m_BorderWidth.Set(3, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetBorderWidthRight() const {
    return m_BorderWidth.Get(1);
}

void Container::SetBorderWidthRight(float width) {
    m_BorderWidth.Set(1, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetBorderWidthTop() const {
    return m_BorderWidth.Get(0);
}

void Container::SetBorderWidthTop(float width) {
    m_BorderWidth.Set(0, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetBorderWidthBottom() const {
    return m_BorderWidth.Get(2);
}

void Container::SetBorderWidthBottom(float width) {
    m_BorderWidth.Set(2, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

Color Container::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void Container::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
    m_MeshNeedsRegeneration = true;
}

bool Container::GetCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Container::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusTopLeft() const {
    return m_CornerRadius.Get(0);
}

void Container::SetCornerRadiusTopLeft(float radius) {
    m_CornerRadius.Set(0, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Container::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Container::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Container::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

// ========================================
// Layout System
// ========================================

bool Container::GetClipChildren() const {
    return m_ClipChildren;
}

void Container::SetClipChildren(bool clip) {
    m_ClipChildren = clip;
    SetPropertyValue<bool>("clipChildren", clip);
}

float Container::GetSeparation() const {
    return m_Separation;
}

void Container::SetSeparation(float separation) {
    m_Separation = separation;
    SetPropertyValue<float>("separation", separation);
    InvalidateLayout();
}

void Container::InvalidateLayout() {
    m_LayoutDirty = true;
    // Anything that dirties the arrangement can also change the size we report upward
    // (a new child, different padding, a Label whose text grew), so drop the memoized
    // minimum for us and every ancestor container.
    InvalidateMeasureCache();
    OnLayoutInvalidated();
}

void Container::ForceLayoutUpdate() {
    RunLayout();
}

void Container::RunLayout() {
    // Re-run until the arrangement stops dirtying itself, so an InvalidateLayout() raised
    // from INSIDE CalculateLayout() is honored instead of being swallowed. The old code
    // cleared m_LayoutDirty after CalculateLayout() unconditionally, which silently
    // dropped exactly those requests (GridContainer raises one in FitChildren mode).
    //
    // Bounded because a container whose measure depends on its own arrangement could
    // otherwise oscillate; two passes settle every layout in this family, and the cap
    // keeps a pathological scene at a fixed cost rather than hanging.
    constexpr int kMaxLayoutPasses = 4;
    for (int pass = 0; pass < kMaxLayoutPasses; ++pass) {
        m_LayoutDirty = false;
        CalculateLayout();
        if (!m_LayoutDirty) {
            return;
        }
    }
    m_LayoutDirty = false;
}

// ========================================
// Rendering Properties
// ========================================

int Container::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Container::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Container::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Container::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

// GetUseUISpace/SetUseUISpace are provided by the UIControl base class
// (registered under the "useUISpace" property name).

// ========================================
// Child Management
// ========================================

std::vector<Node*> Container::GetChildren() const {
    std::vector<Node*> children;

    if (!m_Owner) {
        return children;
    }

    const auto& ownerChildren = m_Owner->GetChildren();
    children.reserve(ownerChildren.size());

    for (const auto& child : ownerChildren) {
        children.push_back(child.get());
    }

    return children;
}

std::vector<Node*> Container::GetVisibleChildren() const {
    std::vector<Node*> visibleChildren;

    if (!m_Owner) {
        return visibleChildren;
    }

    const auto& ownerChildren = m_Owner->GetChildren();

    for (const auto& child : ownerChildren) {
        if (child && child->IsVisible()) {
            visibleChildren.push_back(child.get());
        }
    }

    return visibleChildren;
}

int Container::GetChildCount() const {
    if (!m_Owner) {
        return 0;
    }
    return static_cast<int>(m_Owner->GetChildren().size());
}

Rect Container::GetContentRect() const {
    if (!m_Owner) {
        return Rect();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return Rect();
    }

    Vec2 position = node2D->GetGlobalPosition();
    // Use the resolved on-screen size (anchors / parent-driven) so the content rect
    // matches where the container is actually drawn and hit-tested. Fall back to the
    // requested size only before the first layout pass populates the resolved rect.
    // Deliberately NOT GetBoundsSize(): that clamps up to the content min size, which
    // would collapse a ScrollContainer's viewport onto its full scrollable content.
    // Each axis falls back independently: a container stretched on one axis only (e.g.
    // LeftWide with offsetMax.x == offsetMin.x) resolves to zero on the other, and
    // taking the requested size for that axis alone keeps the content rect valid.
    Vec2 size = GetResolvedRect().size;
    if (size.x <= 0.0f || size.y <= 0.0f) {
        Vec2 requested = GetSize();
        if (size.x <= 0.0f) { size.x = requested.x; }
        if (size.y <= 0.0f) { size.y = requested.y; }
    }

    // Apply padding
    float paddingLeft = GetPaddingLeft();
    float paddingRight = GetPaddingRight();
    float paddingTop = GetPaddingTop();
    float paddingBottom = GetPaddingBottom();

    // The node position is the center of the control, so the minimum (bottom-left in
    // this Y-up canvas) corner is half a size away. paddingTop insets the MAXIMUM-y
    // edge and paddingBottom the MINIMUM-y edge. FromEdges clamps a rect whose padding
    // exceeds its size to zero extent instead of letting it go negative -- a negative
    // content rect otherwise inflates ScrollContainer's scroll range and makes Wrap
    // break a line after every child.
    Rect bounds(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y);

    return Rect::FromEdges(
        bounds.GetLeftEdge() + paddingLeft,
        bounds.GetTopEdge() - paddingTop,
        bounds.GetRightEdge() - paddingRight,
        bounds.GetBottomEdge() + paddingBottom
    );
}

Rect Container::GetOuterRect() const {
    if (!m_Owner) {
        return Rect();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return Rect();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetSize();

    // Apply margin
    float marginLeft = GetMarginLeft();
    float marginRight = GetMarginRight();
    float marginTop = GetMarginTop();
    float marginBottom = GetMarginBottom();

    // As in GetContentRect: the node position is the control's center and the canvas is
    // Y-up, so marginTop expands the MAXIMUM-y edge and marginBottom the MINIMUM-y edge.
    Rect bounds(position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y);

    return Rect::FromEdges(
        bounds.GetLeftEdge() - marginLeft,
        bounds.GetTopEdge() + marginTop,
        bounds.GetRightEdge() + marginRight,
        bounds.GetBottomEdge() - marginBottom
    );
}

// ========================================
// Virtual Layout Methods
// ========================================

void Container::CalculateLayout() {
    // Base implementation does nothing
    // Override in derived classes to implement specific layout algorithms
}

Vec2 Container::GetMinimumSize() const {
    // The floor every derived GetMinimumSize() starts from. This MUST read the raw
    // customMinSize property rather than UIControl::GetMinSize(): GetMinSize() is
    // defined as max(customMinSize, GetContentMinSize()), and Container routes
    // GetContentMinSize() back into GetMinimumSize(), so calling GetMinSize() here would
    // recurse forever.
    return GetCustomMinSize();
}

Vec2 Container::GetContentMinSize() const {
    // The minimum size reported to a parent container is the size required to fit this
    // container's own children (which already includes customMinSize as a floor).
    //
    // Memoized: GetMinSize()/GetBoundsSize() land here, and they run from
    // ContainsCanvasPoint()/GetTopPointerTarget() for every control on every input frame.
    // Uncached, a depth-d nest of non-Fixed containers cost ~2^d full subtree walks per
    // query. The cache is dropped by InvalidateMeasureCache(), which every layout
    // invalidation calls and which propagates up the container chain.
    if (!m_MeasureCacheValid) {
        m_CachedMinimumSize = GetMinimumSize();
        m_MeasureCacheValid = true;
    }
    return m_CachedMinimumSize;
}

void Container::SyncChildListCache() {
    if (!m_Owner) {
        return;
    }
    m_CachedVisibleChildren = GetVisibleChildren();
    m_CachedVisibleChildCount = m_CachedVisibleChildren.size();
    m_CachedChildCount = m_Owner->GetChildCount();
}

void Container::InvalidateMeasureCache() const {
    m_MeasureCacheValid = false;

    // Our content size feeds our parent's, so the whole chain must recompute. Stops at
    // the first non-container ancestor.
    if (!m_Owner) {
        return;
    }
    for (Node* p = m_Owner->GetParent(); p; p = p->GetParent()) {
        std::shared_ptr<UIControl> uc = p->GetComponent<UIControl>();
        if (!uc) {
            continue;
        }
        Container* parentContainer = dynamic_cast<Container*>(uc.get());
        if (!parentContainer) {
            break;
        }
        if (!parentContainer->m_MeasureCacheValid) {
            break;  // already invalid: everything above it is too
        }
        parentContainer->m_MeasureCacheValid = false;
    }
}

// ========================================
// Shared measure / arrange helpers
// ========================================

Vec4 Container::GetChildMargin(core::Node* child) const {
    if (!child) {
        return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // `margin` is a Container property (only a container child can carry one). It is OUTER
    // spacing: space this container reserves AROUND the child, on top of its own size.
    //
    // It used to affect nothing at all -- GetOuterRect() had no callers in core, and the only
    // thing that read the margin was getWorldBounds(), which inflated the editor's selection
    // box by it. So `margin = 20` produced no visual change but made the selection rectangle
    // 40px larger than the drawn panel and picked up clicks outside it.
    if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
        if (const Container* container = dynamic_cast<const Container*>(uc.get())) {
            return container->GetMargin();
        }
    }
    return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

Vec2 Container::GetChildSize(core::Node* child) const {
    if (!child) {
        return Vec2(0.0f, 0.0f);
    }

    // A UIControl child knows its own intrinsic size: the authored width/height floored
    // to whatever its content needs (a Label's measured text, a nested container's
    // children) and clamped to its custom min/max. GetDesiredSize() deliberately ignores
    // the rect this container may have assigned it last pass, so measuring is idempotent.
    //
    // The child's own margin is added on top: it is space this container must reserve, so it
    // belongs in the measurement exactly as the child's size does.
    if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
        const Vec2 desired = uc->GetDesiredSize();
        const Vec4 margin = GetChildMargin(child);   // (top, right, bottom, left)
        return Vec2(desired.x + margin.w + margin.y,
                    desired.y + margin.x + margin.z);
    }

    // Fallback for children with no UIControl: scan components for an explicit size.
    // Prefer a component that actually carries a non-zero size over the merely-first one.
    Vec2 fallback(0.0f, 0.0f);
    bool found = false;
    for (const std::shared_ptr<Component>& component : child->GetComponents()) {
        if (!component) continue;

        const ComponentProperty* widthProp = component->GetPropertyRegistry().GetProperty("width");
        const ComponentProperty* heightProp = component->GetPropertyRegistry().GetProperty("height");
        if (widthProp && heightProp) {
            Vec2 size(widthProp->GetValue<float>(), heightProp->GetValue<float>());
            if (size.x > 0.0f || size.y > 0.0f) {
                return size;
            }
            if (!found) { fallback = size; found = true; }
            continue;
        }

        const ComponentProperty* sizeProp = component->GetPropertyRegistry().GetProperty("size");
        if (sizeProp) {
            Vec2 size = sizeProp->GetValue<Vec2>();
            if (size.x > 0.0f || size.y > 0.0f) {
                return size;
            }
            if (!found) { fallback = size; found = true; }
        }
    }

    return fallback;
}

float Container::GetChildHeightForWidth(core::Node* child, float availableWidth) const {
    if (!child) {
        return 0.0f;
    }

    if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
        // The authored height still acts as a floor, exactly as GetDesiredSize() does on
        // the unconstrained path; height-for-width only ever grows a control (a wrapped
        // Label needs more rows), never shrinks it below what the author asked for.
        float authored = uc->GetSize().y;
        float content = uc->GetContentMinHeightForWidth(availableWidth);
        float height = std::max(authored, content);

        Vec2 maxSz = uc->GetMaxSize();
        if (maxSz.y > 0.0f) {
            height = std::min(height, std::max(maxSz.y, uc->GetMinSize().y));
        }
        return height;
    }

    return GetChildSize(child).y;
}

void Container::SetChildRect(core::Node* child, const math::Rect& globalRect) const {
    if (!child) {
        return;
    }

    // The slot the layout allotted INCLUDES the child's margin (GetChildSize added it), so
    // the child itself is placed in that slot inset by the margin. This is what makes
    // `margin` real outer spacing instead of a number that only inflated the selection box.
    const Vec4 margin = GetChildMargin(child);   // (top, right, bottom, left)
    const math::Rect childRect =
        (margin.x != 0.0f || margin.y != 0.0f || margin.z != 0.0f || margin.w != 0.0f)
            ? math::Rect::FromEdges(globalRect.GetLeftEdge() + margin.w,
                                    globalRect.GetTopEdge() - margin.x,
                                    globalRect.GetRightEdge() - margin.y,
                                    globalRect.GetBottomEdge() + margin.z)
            : globalRect;

    // Preferred path: hand the rect to the child's UIControl. It records it as the
    // resolved (arranged) rect and moves the owning Node2D, WITHOUT writing the authored
    // width/height properties -- see UIControl::SetRect.
    if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
        uc->SetRect(childRect);
        return;
    }

    // Fallback for children with no UIControl. They have nowhere to store a resolved
    // rect, so the size properties are the only channel available; position the Node2D
    // at the rect's center to match the engine's center-pivot convention.
    Node2D* child2D = dynamic_cast<Node2D*>(child);
    if (!child2D) {
        return;
    }

    Vec2 center = childRect.GetCenter();
    Node2D* containerNode = dynamic_cast<Node2D*>(m_Owner);
    child2D->SetPosition(containerNode ? (center - containerNode->GetGlobalPosition()) : center);

    for (const std::shared_ptr<Component>& component : child->GetComponents()) {
        if (!component) continue;

        const ComponentProperty* widthProp = component->GetPropertyRegistry().GetProperty("width");
        const ComponentProperty* heightProp = component->GetPropertyRegistry().GetProperty("height");
        if (widthProp && heightProp) {
            component->SetPropertyValue<float>("width", childRect.size.x);
            component->SetPropertyValue<float>("height", childRect.size.y);
            return;
        }

        const ComponentProperty* sizeProp = component->GetPropertyRegistry().GetProperty("size");
        if (sizeProp) {
            component->SetPropertyValue<Vec2>("size", childRect.size);
            return;
        }
    }
}

void Container::DistributeFreeSpace(MainAxisDistribution mode, float freeSpace, size_t childCount,
                                    float& outLeading, float& outExtraGap) {
    outLeading = 0.0f;
    outExtraGap = 0.0f;

    // Children overflow: there is nothing to distribute, and pretending otherwise would pull
    // them on top of each other.
    if (childCount == 0 || freeSpace <= 0.0f) {
        return;
    }

    const float count = static_cast<float>(childCount);

    switch (mode) {
        case MainAxisDistribution::Center:
            outLeading = freeSpace * 0.5f;
            break;

        case MainAxisDistribution::End:
            outLeading = freeSpace;
            break;

        case MainAxisDistribution::SpaceBetween:
            // Flush at both ends; the slack goes entirely into the gaps. A single child has
            // no gaps, so it simply sits at the start.
            if (childCount > 1) {
                outExtraGap = freeSpace / (count - 1.0f);
            }
            break;

        case MainAxisDistribution::SpaceAround:
            // Each child is given an equal share of space around it, so the two end gaps are
            // half the size of the gaps between children.
            outExtraGap = freeSpace / count;
            outLeading = outExtraGap * 0.5f;
            break;

        case MainAxisDistribution::SpaceEvenly:
            // Every gap -- including the two at the ends -- is the same size.
            outExtraGap = freeSpace / (count + 1.0f);
            outLeading = outExtraGap;
            break;

        case MainAxisDistribution::Begin:
        case MainAxisDistribution::Fill:
        default:
            break;
    }
}

Rect Container::AlignRectIn(const Rect& area, const Vec2& childSize,
                            CrossAxisAlign horizontal, CrossAxisAlign vertical) {
    float width = childSize.x;
    float height = childSize.y;

    if (horizontal == CrossAxisAlign::Fill) { width = area.size.x; }
    if (vertical == CrossAxisAlign::Fill)   { height = area.size.y; }

    width = std::max(0.0f, std::min(width, area.size.x));
    height = std::max(0.0f, std::min(height, area.size.y));

    float x = area.GetLeftEdge();
    switch (horizontal) {
        case CrossAxisAlign::Center: x += (area.size.x - width) * 0.5f; break;
        case CrossAxisAlign::End:    x += (area.size.x - width);        break;
        case CrossAxisAlign::Begin:
        case CrossAxisAlign::Fill:
        default: break;
    }

    // Y-up: Begin is the BOTTOM edge and End is the TOP edge, so the rect's minimum-y
    // corner is measured up from the bottom.
    float y = area.GetBottomEdge();
    switch (vertical) {
        case CrossAxisAlign::Center: y += (area.size.y - height) * 0.5f; break;
        case CrossAxisAlign::End:    y += (area.size.y - height);        break;
        case CrossAxisAlign::Begin:
        case CrossAxisAlign::Fill:
        default: break;
    }

    return Rect(x, y, width, height);
}

Vec2 Container::FitChildSize(core::Node* child, const Vec2& available,
                             bool autoFit, bool maintainAspectRatio) const {
    Vec2 desired = GetChildSize(child);

    if (!autoFit) {
        return desired;
    }

    // A child with no intrinsic size takes the whole area.
    if (desired.x <= 0.0f || desired.y <= 0.0f) {
        return available;
    }

    if (!maintainAspectRatio) {
        return available;
    }

    // Largest box with the child's aspect ratio that fits. Computed from the child's
    // DESIRED size every pass, so it is idempotent: the old code wrote the fitted size
    // back into the child's width/height and re-read it next frame, so shrinking and
    // re-growing the container left the child inflated (an aspect-preserved fit of a fit).
    const float scale = std::min(available.x / desired.x, available.y / desired.y);
    return Vec2(desired.x * scale, desired.y * scale);
}

void Container::PlaceChildOnCrossAxis(core::Node* child, bool vertical,
                                      float axisBegin, float axisExtent, float desiredExtent,
                                      CrossAxisAlign fallback,
                                      float& outBegin, float& outExtent) const {
    // A UIControl child's own size flags are authoritative on the cross axis -- the Godot
    // model. Previously ChildWantsFill() had zero call sites, so sizeFlagsHorizontal /
    // sizeFlagsVertical did nothing outside HBox/VBox and the ONLY way to fill was a
    // container-wide alignment enum that then force-filled every child alike.
    //
    // Note SizeFlagBits::ShrinkBegin is 0 (a sentinel, not a bit): "no bits set" IS the
    // explicit shrink-to-begin request, so it must not fall through to the fallback.
    // The container-wide alignment therefore only applies to children with no UIControl,
    // which have no flags to state a preference with.
    CrossAxisAlign align = fallback;

    if (child) {
        if (std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>()) {
            const int flags = vertical ? uc->GetSizeFlagsVertical() : uc->GetSizeFlagsHorizontal();

            // The size flags are named in READING order -- ShrinkBegin means left on the
            // horizontal axis and TOP on the vertical one, as in Godot. CrossAxisAlign, by
            // contrast, is named for the canvas axis, where Begin is the LOW edge and the
            // low edge of a Y-up axis is the bottom. The two therefore have to be flipped
            // against each other on Y; mapping ShrinkBegin straight onto Begin would send a
            // control that asked to sit at the top of an HBox row to the bottom of it.
            if (flags & SizeFlagBits::Fill) {
                align = CrossAxisAlign::Fill;
            } else if (flags & SizeFlagBits::ShrinkCenter) {
                align = CrossAxisAlign::Center;
            } else if (flags & SizeFlagBits::ShrinkEnd) {
                align = vertical ? CrossAxisAlign::Begin : CrossAxisAlign::End;
            } else {
                align = vertical ? CrossAxisAlign::End : CrossAxisAlign::Begin;
            }
        }
    }

    // Never hand back a negative extent, and never overflow the available span.
    const float extent = std::max(0.0f, std::min(desiredExtent, axisExtent));

    switch (align) {
        case CrossAxisAlign::Fill:
            outBegin = axisBegin;
            outExtent = std::max(0.0f, axisExtent);
            return;
        case CrossAxisAlign::Center:
            outBegin = axisBegin + (axisExtent - extent) * 0.5f;
            outExtent = extent;
            return;
        case CrossAxisAlign::End:
            outBegin = axisBegin + (axisExtent - extent);
            outExtent = extent;
            return;
        case CrossAxisAlign::Begin:
        default:
            outBegin = axisBegin;
            outExtent = extent;
            return;
    }
}

bool Container::ChildWantsExpand(core::Node* child, bool vertical) const {
    if (!child) {
        return false;
    }

    // Generic UIControl size flags.
    std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>();
    if (uc) {
        int flags = vertical ? uc->GetSizeFlagsVertical() : uc->GetSizeFlagsHorizontal();
        if (flags & SizeFlagBits::Expand) {
            return true;
        }
    }

    // Legacy Spacer "expand" property (kept for backward compatibility).
    for (const auto& component : child->GetComponents()) {
        if (component && component->GetTypeName() == "Spacer") {
            const ComponentProperty* expandProp = component->GetPropertyRegistry().GetProperty("expand");
            if (expandProp && expandProp->GetValue<bool>()) {
                return true;
            }
        }
    }

    return false;
}

bool Container::ChildWantsFill(core::Node* child, bool vertical) const {
    if (!child) {
        return false;
    }
    std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>();
    if (uc) {
        int flags = vertical ? uc->GetSizeFlagsVertical() : uc->GetSizeFlagsHorizontal();
        return (flags & SizeFlagBits::Fill) != 0;
    }
    return false;
}

std::vector<float> Container::DistributeMainAxis(const std::vector<core::Node*>& children,
                                                 bool vertical,
                                                 float availableMain,
                                                 float separation) const {
    const size_t count = children.size();
    std::vector<float> sizes(count, 0.0f);
    if (count == 0) {
        return sizes;
    }

    // Per-child bounds on the main axis. A child can never be arranged below its minimum
    // or above its maximum, no matter how the stretch ratios fall out -- the old code
    // applied no clamp at all, so a child with customMaxSize.x = 50 was still stretched
    // to 300.
    std::vector<float> minSizes(count, 0.0f);
    std::vector<float> maxSizes(count, 0.0f);   // 0 == unbounded
    std::vector<bool> expanding(count, false);
    std::vector<float> ratios(count, 1.0f);
    std::vector<bool> pinned(count, false);

    float desiredTotal = 0.0f;
    float pool = 0.0f;
    float ratioTotal = 0.0f;

    for (size_t i = 0; i < count; ++i) {
        core::Node* child = children[i];
        const Vec2 desired = GetChildSize(child);
        sizes[i] = vertical ? desired.y : desired.x;

        if (std::shared_ptr<UIControl> uc = child ? child->GetComponent<UIControl>() : nullptr) {
            const Vec2 mn = uc->GetMinSize();
            const Vec2 mx = uc->GetMaxSize();
            minSizes[i] = vertical ? mn.y : mn.x;
            maxSizes[i] = vertical ? mx.y : mx.x;
        }

        desiredTotal += sizes[i];

        if (ChildWantsExpand(child, vertical)) {
            expanding[i] = true;
            ratios[i] = ChildStretchRatio(child);
            pool += sizes[i];
            ratioTotal += ratios[i];
        }
    }

    const float totalSeparation = separation * static_cast<float>(count - 1);
    // Free space may legitimately be NEGATIVE when the children overflow the container.
    // It is not clamped to zero here: clamping is what made a child able to grow but
    // never shrink, so once a container was made smaller its children stayed inflated.
    const float freeSpace = availableMain - totalSeparation - desiredTotal;

    if (!(ratioTotal > 0.0f)) {
        // Nothing expands: every child keeps its desired extent (clamped to its own
        // bounds). The caller distributes any free space via the main-axis alignment.
        for (size_t i = 0; i < count; ++i) {
            sizes[i] = std::max(sizes[i], minSizes[i]);
            if (maxSizes[i] > 0.0f) {
                sizes[i] = std::min(sizes[i], std::max(maxSizes[i], minSizes[i]));
            }
            sizes[i] = std::max(0.0f, sizes[i]);
        }
        return sizes;
    }

    // The expanding children share (their own desired extents + all the free space),
    // split by stretch ratio. Iterate: whenever a child's share violates its own min or
    // max we pin it there, remove it from the pool, and re-split among the rest.
    pool += freeSpace;

    for (size_t guard = 0; guard <= count; ++guard) {
        bool repin = false;

        for (size_t i = 0; i < count; ++i) {
            if (!expanding[i] || pinned[i]) {
                continue;
            }
            float share = (ratioTotal > 0.0f) ? (pool * ratios[i] / ratioTotal) : 0.0f;

            float clamped = std::max(share, minSizes[i]);
            if (maxSizes[i] > 0.0f) {
                clamped = std::min(clamped, std::max(maxSizes[i], minSizes[i]));
            }
            clamped = std::max(0.0f, clamped);

            if (std::abs(clamped - share) > 0.001f) {
                pinned[i] = true;
                sizes[i] = clamped;
                pool -= clamped;
                ratioTotal -= ratios[i];
                repin = true;
                break;
            }
            sizes[i] = std::max(0.0f, share);
        }

        if (!repin) {
            break;
        }
        if (!(ratioTotal > 0.0f)) {
            break;
        }
    }

    // Non-expanding children keep their desired extent, clamped to their own bounds.
    for (size_t i = 0; i < count; ++i) {
        if (expanding[i] && !pinned[i]) {
            continue;
        }
        if (!expanding[i]) {
            sizes[i] = std::max(sizes[i], minSizes[i]);
            if (maxSizes[i] > 0.0f) {
                sizes[i] = std::min(sizes[i], std::max(maxSizes[i], minSizes[i]));
            }
            sizes[i] = std::max(0.0f, sizes[i]);
        }
    }

    return sizes;
}

float Container::ChildStretchRatio(core::Node* child) const {
    if (!child) {
        return 1.0f;
    }
    std::shared_ptr<UIControl> uc = child->GetComponent<UIControl>();
    if (uc) {
        float ratio = uc->GetSizeFlagsStretchRatio();
        return ratio > 0.0f ? ratio : 1.0f;
    }
    return 1.0f;
}

void Container::OnLayoutInvalidated() {
    // Base implementation does nothing
    // Override in derived classes for custom behavior
}

// ========================================
// Internal Rendering Methods
// ========================================

void Container::RenderBackground(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    if (!GetDrawBackground()) {
        return;
    }

    Color bgColor = GetBackgroundColor();
    bgColor.a *= GetOpacity();

    if (bgColor.a <= 0.0f) {
        return;
    }

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    int blendMode = 0;

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(position, size, cornerRadius, bgColor, rotation, blendMode);
    } else {
        // Position is center-pivot, convert to top-left for non-rotated rendering
        Vec2 topLeft = Vec2(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, bgColor, blendMode);
    }
}

void Container::RenderBorder(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    if (!GetBorderEnabled()) {
        return;
    }

    float borderTop = m_BorderWidth.Get(0);
    float borderRight = m_BorderWidth.Get(1);
    float borderBottom = m_BorderWidth.Get(2);
    float borderLeft = m_BorderWidth.Get(3);

    if (borderTop <= 0.0f && borderRight <= 0.0f && borderBottom <= 0.0f && borderLeft <= 0.0f) {
        return;
    }

    Color borderColor = GetBorderColor();
    Vec4 innerRadius = m_CornerRadius.AsVec4();

    Vec2 outerSize = Vec2(size.x + borderLeft + borderRight, size.y + borderTop + borderBottom);
    Vec2 outerPosition = Vec2(position.x, position.y);

    Vec4 outerRadius = Vec4(
        innerRadius.x + std::max(borderTop, borderLeft),
        innerRadius.y + std::max(borderTop, borderRight),
        innerRadius.z + std::max(borderBottom, borderRight),
        innerRadius.w + std::max(borderBottom, borderLeft)
    );

    Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRectBorder(outerPosition, outerSize, outerRadius, borderWidthVec, borderColor, rotation);
    } else {
        // Position is center-pivot, convert to top-left for non-rotated rendering
        Vec2 topLeft = Vec2(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
        Vec2 outerPosNoRot = Vec2(topLeft.x - borderLeft, topLeft.y - borderTop);
        ctx.drawRoundedRectBorder(outerPosNoRot, outerSize, outerRadius, borderWidthVec, borderColor);
    }
}

// ========================================
// Internal Helper Methods
// ========================================

Vec2 Container::CalculateFinalSize() const {
    Vec2 baseSize(GetPropertyValue<float>("width"), GetPropertyValue<float>("height"));
    Vec2 finalSize = baseSize;

    // Calculate width based on horizontal size mode
    if (m_HorizontalSizeMode == SizeMode::FitChildren) {
        Vec2 childrenBounds = CalculateChildrenBounds();
        finalSize.x = childrenBounds.x + GetPaddingLeft() + GetPaddingRight();
    } else if (m_HorizontalSizeMode == SizeMode::Minimum) {
        Vec2 minSize = GetMinimumSize();
        finalSize.x = minSize.x;
    }
    // Expand mode would be handled by parent container

    // Calculate height based on vertical size mode
    if (m_VerticalSizeMode == SizeMode::FitChildren) {
        Vec2 childrenBounds = CalculateChildrenBounds();
        finalSize.y = childrenBounds.y + GetPaddingTop() + GetPaddingBottom();
    } else if (m_VerticalSizeMode == SizeMode::Minimum) {
        Vec2 minSize = GetMinimumSize();
        finalSize.y = minSize.y;
    }

    // Apply size constraints
    finalSize = ApplySizeConstraints(finalSize);

    return finalSize;
}

Vec2 Container::ApplySizeConstraints(const Vec2& size) const {
    // One constraint source (customMinSize/customMaxSize on UIControl), one convention:
    // a maximum of 0 on an axis means "unbounded" and the minimum always wins. The old
    // code clamped against Container's own shadowing minSize/maxSize properties and
    // treated max literally, so a max of 0 would have collapsed the container.
    return ClampToMinMax(size);
}

Vec2 Container::CalculateChildrenBounds() const {
    std::vector<Node*> visibleChildren = GetVisibleChildren();

    if (visibleChildren.empty()) {
        return Vec2(0.0f, 0.0f);
    }

    // Union of the children's INTRINSIC sizes. The previous implementation hardcoded
    // every child to 1x1 pixel ("derived containers may override this" -- none did, and
    // it was not even virtual), so SizeMode::FitChildren collapsed every container to
    // roughly its padding. It also derived the bounds from child global positions that
    // this container itself had written on the previous pass, which is a feedback loop.
    Vec2 bounds(0.0f, 0.0f);
    for (Node* child : visibleChildren) {
        Vec2 childSize = GetChildSize(child);
        bounds.x = std::max(bounds.x, childSize.x);
        bounds.y = std::max(bounds.y, childSize.y);
    }

    return bounds;
}

void Container::SyncFromProperties() {
    // Sync padding
    Vec4 paddingVec = GetPropertyValue<Vec4>("padding");
    bool paddingLinked = GetPropertyValue<bool>("paddingLinked");
    if (paddingVec != m_Padding.AsVec4() || paddingLinked != m_Padding.IsLinked()) {
        m_Padding.FromVec4(paddingVec);
        m_Padding.SetLinked(paddingLinked);
        InvalidateLayout();
    }

    // Sync margin
    Vec4 marginVec = GetPropertyValue<Vec4>("margin");
    bool marginLinked = GetPropertyValue<bool>("marginLinked");
    if (marginVec != m_Margin.AsVec4() || marginLinked != m_Margin.IsLinked()) {
        m_Margin.FromVec4(marginVec);
        m_Margin.SetLinked(marginLinked);
        InvalidateLayout();
    }

    // Sync border width
    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    if (borderWidthVec != m_BorderWidth.AsVec4() || borderWidthLinked != m_BorderWidth.IsLinked()) {
        m_BorderWidth.FromVec4(borderWidthVec);
        m_BorderWidth.SetLinked(borderWidthLinked);
        m_MeshNeedsRegeneration = true;
    }

    // Sync corner radius
    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    if (cornerRadiusVec != m_CornerRadius.AsVec4() || cornerRadiusLinked != m_CornerRadius.IsLinked()) {
        m_CornerRadius.FromVec4(cornerRadiusVec);
        m_CornerRadius.SetLinked(cornerRadiusLinked);
        m_MeshNeedsRegeneration = true;
    }

    // Sync size modes
    SizeMode horizontalMode = static_cast<SizeMode>(GetPropertyValue<int>("horizontalSizeMode"));
    SizeMode verticalMode = static_cast<SizeMode>(GetPropertyValue<int>("verticalSizeMode"));
    if (horizontalMode != m_HorizontalSizeMode || verticalMode != m_VerticalSizeMode) {
        m_HorizontalSizeMode = horizontalMode;
        m_VerticalSizeMode = verticalMode;
        InvalidateLayout();
    }

    // Sync layout properties
    bool clipChildren = GetPropertyValue<bool>("clipChildren");
    float separation = GetPropertyValue<float>("separation");
    if (clipChildren != m_ClipChildren || separation != m_Separation) {
        m_ClipChildren = clipChildren;
        m_Separation = separation;
        InvalidateLayout();
    }

    // Give the derived container a chance to refresh its own cached properties. Without
    // this, those members are only ever written by OnAwake()/OnPropertyChanged() -- and
    // the editor never calls OnAwake(), so a reopened scene laid out with constructor
    // defaults rather than its serialized values.
    SyncDerivedProperties();
}

bool Container::SyncCachedFloat(const std::string& propertyName, float& cached) {
    const float value = GetPropertyValue<float>(propertyName);
    if (value == cached) {
        return false;
    }
    cached = value;
    InvalidateLayout();
    return true;
}

bool Container::SyncCachedInt(const std::string& propertyName, int& cached) {
    const int value = GetPropertyValue<int>(propertyName);
    if (value == cached) {
        return false;
    }
    cached = value;
    InvalidateLayout();
    return true;
}

bool Container::SyncCachedBool(const std::string& propertyName, bool& cached) {
    const bool value = GetPropertyValue<bool>(propertyName);
    if (value == cached) {
        return false;
    }
    cached = value;
    InvalidateLayout();
    return true;
}

bool Container::SyncCachedColor(const std::string& propertyName, Color& cached) {
    const Color value = GetPropertyValue<Color>(propertyName);
    if (value == cached) {
        return false;
    }
    cached = value;
    m_MeshNeedsRegeneration = true;
    return true;
}

// ========================================
// IRenderableComponent Implementation
// ========================================

void Container::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    Node* owner = GetOwner();
    if (!owner) return;

    Node2D* node2D = dynamic_cast<Node2D*>(owner);
    if (!node2D) return;

    // NOTE: layout is NOT resolved here. The draw pass is sorted by layer/sortingOrder,
    // which gives no parent-before-child guarantee, so resolving (and therefore writing
    // node transforms) from draw meant a child could lay itself out against its parent's
    // PREVIOUS-frame rect. Layout now runs exclusively in the update pass -- see
    // UIControl::LayoutPass(), which walks the tree top-down before rendering.
    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    float rotation = node2D->GetGlobalRotation();

    // Cache position/size purely to decide whether the background mesh must be rebuilt.
    // These no longer invalidate layout: doing so from the draw pass is what made layout
    // and rendering race in the first place.
    if (position != m_CachedPosition) {
        m_CachedPosition = position;
        m_MeshNeedsRegeneration = true;
    }
    if (size != m_CachedSize) {
        m_CachedSize = size;
        m_MeshNeedsRegeneration = true;
    }

    // A theme may supply a uniform corner radius. Applied only when the theme
    // defines it and the property is not a local override (preserving per-corner
    // values otherwise).
    if (!IsThemeOverridden("cornerRadius")) {
        const ui::ThemeAsset* theme = GetEffectiveTheme();
        ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
        if (tm.HasConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius")) {
            float r = tm.ResolveConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius", 0.0f);
            m_CornerRadius.FromVec4(Vec4(r, r, r, r));
            m_CornerRadius.SetLinked(true);
        }
    }

    // Render background and border
    RenderBorder(ctx, position, size, rotation);
    RenderBackground(ctx, position, size, rotation);

    // Child clipping (when clipChildren is enabled) is applied by the render
    // traversal via ClipsDescendants()/GetClipRect(), which push a scissor clip
    // around this node's descendants. No work is needed here.
}

AABB Container::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    Node2D* node2d = dynamic_cast<Node2D*>(GetOwner());
    if (!node2d) {
        return AABB();
    }

    Vec2 globalPos = node2d->GetGlobalPosition();
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    // The margin is NOT included here. It is outer spacing that a parent container reserves
    // AROUND this one (see GetChildMargin / SetChildRect), not part of the container itself:
    // inflating the bounds by it made the editor's selection box larger than the drawn panel
    // and picked up clicks outside it, while doing nothing whatsoever to the layout.
    Vec2 sizeWithMargin = GetBoundsSize();
    sizeWithMargin.x *= globalScale.x;
    sizeWithMargin.y *= globalScale.y;

    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        Vec2 halfSize = sizeWithMargin * 0.5f;

        Vec2 localCorners[4] = {
            Vec2(-halfSize.x, -halfSize.y),
            Vec2( halfSize.x, -halfSize.y),
            Vec2( halfSize.x,  halfSize.y),
            Vec2(-halfSize.x,  halfSize.y)
        };

        Vec2 min(FLT_MAX, FLT_MAX);
        Vec2 max(-FLT_MAX, -FLT_MAX);

        for (int i = 0; i < 4; ++i) {
            Vec2 rotated(
                localCorners[i].x * cosR - localCorners[i].y * sinR,
                localCorners[i].x * sinR + localCorners[i].y * cosR
            );
            Vec2 worldCorner = globalPos + rotated;
            min.x = std::min(min.x, worldCorner.x);
            min.y = std::min(min.y, worldCorner.y);
            max.x = std::max(max.x, worldCorner.x);
            max.y = std::max(max.y, worldCorner.y);
        }

        return AABB(Vec3(min.x, min.y, -0.1f), Vec3(max.x, max.y, 0.1f));
    }

    Vec3 min(globalPos.x - sizeWithMargin.x * 0.5f, globalPos.y - sizeWithMargin.y * 0.5f, -0.1f);
    Vec3 max(globalPos.x + sizeWithMargin.x * 0.5f, globalPos.y + sizeWithMargin.y * 0.5f, 0.1f);
    return AABB(min, max);
}

math::OBB Container::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    Node2D* node2d = dynamic_cast<Node2D*>(GetOwner());
    if (!node2d) {
        return math::OBB();
    }

    Vec2 position = node2d->GetGlobalPosition();
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    // As in getWorldBounds: the margin is outer spacing a PARENT container reserves around
    // this one, not part of the container's own box, so it must not inflate the bounds.
    Vec2 sizeWithMargin = GetBoundsSize();
    sizeWithMargin.x *= globalScale.x;
    sizeWithMargin.y *= globalScale.y;

    Vec3 center = Vec3(position.x, position.y, 0.0f);
    Vec3 extents = Vec3(sizeWithMargin.x * 0.5f, sizeWithMargin.y * 0.5f, 0.1f);
    Quat quatRotation = Quat::FromAxisAngle(Vec3::UnitZ(), rotation);

    return math::OBB(center, extents, quatRotation);
}

bool Container::IntersectRay(const math::Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

RenderLayer Container::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();
    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType Container::getSpatialType() const {
    return GetUISpatialType();
}

} // namespace components
} // namespace lupine
