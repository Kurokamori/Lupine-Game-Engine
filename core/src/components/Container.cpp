#include "lupine/components/Container.hpp"
#include "lupine/core/Node.hpp"
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
    : Component("Container")
    , m_LayoutDirty(true)
    , m_CachedSize(100.0f, 100.0f)
    , m_CachedPosition(0.0f, 0.0f)
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
    : Component(name)
    , m_LayoutDirty(true)
    , m_CachedSize(100.0f, 100.0f)
    , m_CachedPosition(0.0f, 0.0f)
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
    // Size properties
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 100.0f, 0.0f, 10000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 100.0f, 0.0f, 10000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(minSize, Vec2, Vec2(0.0f, 0.0f), "Size"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(maxSize, Vec2, Vec2(10000.0f, 10000.0f), "Size"));
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
    DefineProperty(PROPERTY_DEFAULT_GROUP(useUISpace, Bool, true, "Rendering"));
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

void Container::OnUpdate(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    SyncFromProperties();

    // Recalculate layout if dirty
    if (m_LayoutDirty) {
        CalculateLayout();
        m_LayoutDirty = false;
    }
}

bool Container::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (!is3D) {
        float currentWidth = GetWidth();
        float currentHeight = GetHeight();

        if (axis == 0) {
            // Scale width
            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
        } else if (axis == 1) {
            // Scale height
            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        } else if (axis == -1) {
            // Uniform scale
            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        }

        InvalidateLayout();
        m_MeshNeedsRegeneration = true;
        return true;
    }

    return false;
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
    InvalidateLayout();
}

float Container::GetHeight() const {
    if (m_VerticalSizeMode != SizeMode::Fixed) {
        return CalculateFinalSize().y;
    }
    return GetPropertyValue<float>("height");
}

void Container::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    InvalidateLayout();
}

Vec2 Container::GetSize() const {
    return Vec2(GetWidth(), GetHeight());
}

void Container::SetSize(const Vec2& size) {
    SetWidth(size.x);
    SetHeight(size.y);
}

Vec2 Container::GetMinSize() const {
    return GetPropertyValue<Vec2>("minSize");
}

void Container::SetMinSize(const Vec2& minSize) {
    SetPropertyValue<Vec2>("minSize", minSize);
    InvalidateLayout();
}

Vec2 Container::GetMaxSize() const {
    return GetPropertyValue<Vec2>("maxSize");
}

void Container::SetMaxSize(const Vec2& maxSize) {
    SetPropertyValue<Vec2>("maxSize", maxSize);
    InvalidateLayout();
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

const Color& Container::GetBackgroundColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("backgroundColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::White();
}

void Container::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
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

const Color& Container::GetBorderColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::Black();
}

void Container::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
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
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Container::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Container::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Container::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Container::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
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
    OnLayoutInvalidated();
}

void Container::ForceLayoutUpdate() {
    CalculateLayout();
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

bool Container::GetUseUISpace() const {
    return GetPropertyValue<bool>("useUISpace");
}

void Container::SetUseUISpace(bool useUISpace) {
    SetPropertyValue<bool>("useUISpace", useUISpace);
}

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
    Vec2 size = GetSize();

    // Apply padding
    float paddingLeft = GetPaddingLeft();
    float paddingRight = GetPaddingRight();
    float paddingTop = GetPaddingTop();
    float paddingBottom = GetPaddingBottom();

    Rect contentRect;
    contentRect.position = Vec2(position.x + paddingLeft, position.y + paddingTop);
    contentRect.size = Vec2(
        size.x - paddingLeft - paddingRight,
        size.y - paddingTop - paddingBottom
    );

    return contentRect;
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

    Rect outerRect;
    outerRect.position = Vec2(position.x - marginLeft, position.y - marginTop);
    outerRect.size = Vec2(
        size.x + marginLeft + marginRight,
        size.y + marginTop + marginBottom
    );

    return outerRect;
}

// ========================================
// Virtual Layout Methods
// ========================================

void Container::CalculateLayout() {
    // Base implementation does nothing
    // Override in derived classes to implement specific layout algorithms
}

Vec2 Container::GetMinimumSize() const {
    // Base implementation returns the minSize property
    return GetMinSize();
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
        ctx.drawRoundedRect(position, size, cornerRadius, bgColor, blendMode);
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
        Vec2 outerPosNoRot = Vec2(position.x - borderLeft, position.y - borderTop);
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
    Vec2 minSize = GetMinSize();
    Vec2 maxSize = GetMaxSize();

    Vec2 constrainedSize = size;
    constrainedSize.x = std::max(minSize.x, std::min(maxSize.x, size.x));
    constrainedSize.y = std::max(minSize.y, std::min(maxSize.y, size.y));

    return constrainedSize;
}

Vec2 Container::CalculateChildrenBounds() const {
    std::vector<Node*> visibleChildren = GetVisibleChildren();

    if (visibleChildren.empty()) {
        return Vec2(0.0f, 0.0f);
    }

    // Calculate bounding box of all children
    Vec2 minPos(FLT_MAX, FLT_MAX);
    Vec2 maxPos(-FLT_MAX, -FLT_MAX);

    for (Node* child : visibleChildren) {
        Node2D* child2D = dynamic_cast<Node2D*>(child);
        if (!child2D) continue;

        Vec2 childPos = child2D->GetGlobalPosition();
        Vec2 childScale = child2D->GetGlobalScale();

        // Assume a default size of 1x1 if we can't determine actual size
        // Derived containers may override this to get actual child sizes
        Vec2 childSize(1.0f, 1.0f);

        minPos.x = std::min(minPos.x, childPos.x);
        minPos.y = std::min(minPos.y, childPos.y);
        maxPos.x = std::max(maxPos.x, childPos.x + childSize.x * childScale.x);
        maxPos.y = std::max(maxPos.y, childPos.y + childSize.y * childScale.y);
    }

    return Vec2(maxPos.x - minPos.x, maxPos.y - minPos.y);
}

bool Container::IsPointInside(const Vec2& point) const {
    if (!m_Owner) {
        return false;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetSize();

    return point.x >= position.x && point.x <= position.x + size.x &&
           point.y >= position.y && point.y <= position.y + size.y;
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

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetSize();
    float rotation = node2D->GetGlobalRotation();

    // Cache position for change detection
    if (position != m_CachedPosition) {
        m_CachedPosition = position;
        m_MeshNeedsRegeneration = true;
    }

    // Cache size for change detection
    Vec2 currentSize = GetSize();
    if (currentSize != m_CachedSize) {
        m_CachedSize = currentSize;
        InvalidateLayout();
        m_MeshNeedsRegeneration = true;
    }

    // Render background and border
    RenderBorder(ctx, position, size, rotation);
    RenderBackground(ctx, position, size, rotation);

    // TODO: If clip children is enabled, set up clipping region
    // This would require render context support for scissor testing or stencil buffer
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

    Vec2 size = GetSize();
    size.x *= globalScale.x;
    size.y *= globalScale.y;

    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        Vec2 halfSize = size * 0.5f;

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

    Vec3 min(globalPos.x - size.x * 0.5f, globalPos.y - size.y * 0.5f, -0.1f);
    Vec3 max(globalPos.x + size.x * 0.5f, globalPos.y + size.y * 0.5f, 0.1f);
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
    Vec2 size = GetSize();
    Vec2 globalScale = node2d->GetGlobalScale();
    float rotation = node2d->GetGlobalRotation();

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    Vec3 center = Vec3(position.x, position.y, 0.0f);
    Vec3 extents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.1f);
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
    if (GetUseUISpace()) {
        return SpatialType::Canvas;
    } else {
        return SpatialType::World2D;
    }
}

} // namespace components
} // namespace lupine
