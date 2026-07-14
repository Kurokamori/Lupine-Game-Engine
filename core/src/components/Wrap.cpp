#include "lupine/components/Wrap.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Wrap::Wrap()
    : Container("Wrap")
    , m_SpacingX(0.0f)
    , m_SpacingY(0.0f)
    , m_LineAlignment(LineAlignment::Left)
    , m_MaxLines(0)
    , m_WrapDirection(WrapDirection::Horizontal)
{
}

Wrap::Wrap(const std::string& name)
    : Container(name)
    , m_SpacingX(0.0f)
    , m_SpacingY(0.0f)
    , m_LineAlignment(LineAlignment::Left)
    , m_MaxLines(0)
    , m_WrapDirection(WrapDirection::Horizontal)
{
}

Wrap::~Wrap() {
}

void Wrap::DefineProperties() {
    // Call parent to define base container properties
    Container::DefineProperties();

    // Define Wrap-specific properties
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spacingX, 0.0f, 0.0f, 100.0f, 1.0f, "Wrap"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spacingY, 0.0f, 0.0f, 100.0f, 1.0f, "Wrap"));
    DefineProperty(PROPERTY_ENUM_GROUP(lineAlignment, 0, "Wrap", Left, Center, Right, Justify));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxLines, 0, 0, 100, 1, "Wrap"));
    DefineProperty(PROPERTY_ENUM_GROUP(wrapDirection, 0, "Wrap", Horizontal, Vertical));
}

void Wrap::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
}

void Wrap::OnReady() {
    Container::OnReady();

    // Force initial layout calculation
    InvalidateLayout();
}

void Wrap::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties(), which calls SyncDerivedProperties() -- so the
    // Wrap-specific members below are refreshed from that one path rather than from a
    // parallel if/else chain here that the editor's load path never reached.
    Container::OnPropertyChanged(propertyName, newValue);
}

void Wrap::SyncDerivedProperties() {
    SyncCachedFloat("spacingX", m_SpacingX);
    SyncCachedFloat("spacingY", m_SpacingY);
    SyncCachedEnum("lineAlignment", m_LineAlignment);
    SyncCachedInt("maxLines", m_MaxLines);
    SyncCachedEnum("wrapDirection", m_WrapDirection);
}

// ========================================
// Wrap Specific Properties
// ========================================

float Wrap::GetSpacingX() const {
    return m_SpacingX;
}

void Wrap::SetSpacingX(float spacing) {
    m_SpacingX = spacing;
    SetPropertyValue<float>("spacingX", spacing);
    InvalidateLayout();
}

float Wrap::GetSpacingY() const {
    return m_SpacingY;
}

void Wrap::SetSpacingY(float spacing) {
    m_SpacingY = spacing;
    SetPropertyValue<float>("spacingY", spacing);
    InvalidateLayout();
}

Wrap::LineAlignment Wrap::GetLineAlignment() const {
    return m_LineAlignment;
}

void Wrap::SetLineAlignment(LineAlignment alignment) {
    m_LineAlignment = alignment;
    SetPropertyValue<int>("lineAlignment", static_cast<int>(alignment));
    InvalidateLayout();
}

int Wrap::GetMaxLines() const {
    return m_MaxLines;
}

void Wrap::SetMaxLines(int maxLines) {
    m_MaxLines = maxLines;
    SetPropertyValue<int>("maxLines", maxLines);
    InvalidateLayout();
}

Wrap::WrapDirection Wrap::GetWrapDirection() const {
    return m_WrapDirection;
}

void Wrap::SetWrapDirection(WrapDirection direction) {
    m_WrapDirection = direction;
    SetPropertyValue<int>("wrapDirection", static_cast<int>(direction));
    InvalidateLayout();
}

// ========================================
// Container Virtual Method Overrides
// ========================================

void Wrap::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    const bool vertical = (m_WrapDirection == WrapDirection::Vertical);
    const Rect contentArea = GetContentRect();

    std::vector<Node*> overflow;
    const std::vector<Run> runs = BuildRuns(
        vertical,
        vertical ? contentArea.size.y : contentArea.size.x,
        &overflow);

    // Spacing BETWEEN runs is the cross-axis spacing: rows are separated vertically, and
    // columns horizontally.
    const float runSpacing = vertical ? m_SpacingX : m_SpacingY;

    // Runs stack from the content area's TOP edge downward (horizontal wrap) or from its
    // LEFT edge rightward (vertical wrap).
    //
    // The old code started at `contentArea.position.y` -- the MINIMUM y, which on this
    // Y-up canvas is the BOTTOM -- and then ADDED the line height to advance, so the first
    // line was drawn along the bottom edge and every subsequent line marched upward, out
    // through the top of the container.
    float runCursor = vertical ? contentArea.GetLeftEdge() : contentArea.GetTopEdge();

    for (const Run& run : runs) {
        const float runBegin = vertical ? runCursor : (runCursor - run.crossExtent);
        ArrangeRun(run, vertical, contentArea, runBegin, run.crossExtent);

        if (vertical) {
            runCursor += run.crossExtent + runSpacing;
        } else {
            runCursor -= run.crossExtent + runSpacing;
        }
    }

    // Children pushed past maxLines are collapsed to an empty rect at the content origin
    // rather than being left wherever a previous pass happened to put them -- otherwise
    // lowering maxLines leaves orphans drawn over the runs that survived.
    for (Node* child : overflow) {
        SetChildRect(child, Rect(contentArea.GetTopLeft(), Vec2(0.0f, 0.0f)));
    }
}

math::Vec2 Wrap::GetMinimumSize() const {
    // The authored customMinSize is a FLOOR on the whole container; padding is not added on
    // top of it. This override used to ignore its children ENTIRELY and return
    // minSize + padding, so a Wrap nested in a VBox was allocated nothing and spilled out.
    const Vec2 floorSize = Container::GetMinimumSize();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    const bool vertical = (m_WrapDirection == WrapDirection::Vertical);

    // Height-for-width: how tall the Wrap needs to be depends on how wide it is allowed to
    // be, because that decides how many runs the children break into. Measure against the
    // extent currently available along the run axis, which is the resolved rect minus
    // padding; before the first arrange (or when unconstrained) BuildRuns yields one run,
    // i.e. the natural single-line size.
    const Vec2 resolved = GetResolvedRect().size;
    const float availableMain = vertical ? (resolved.y - padY) : (resolved.x - padX);

    const std::vector<Run> runs = BuildRuns(vertical, availableMain, nullptr);

    // `main` runs along each run; `cross` accumulates across them.
    float main = 0.0f;
    float cross = 0.0f;
    for (const Run& run : runs) {
        main = std::max(main, run.mainExtent);
        cross += run.crossExtent;
    }
    if (runs.size() > 1) {
        cross += (vertical ? m_SpacingX : m_SpacingY) * static_cast<float>(runs.size() - 1);
    }

    const Vec2 content = vertical ? Vec2(cross, main) : Vec2(main, cross);

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void Wrap::OnLayoutInvalidated() {
    // Base implementation
    Container::OnLayoutInvalidated();
}

// ========================================
// Internal Helper Methods
// ========================================

std::vector<Wrap::Run> Wrap::BuildRuns(bool vertical, float availableMain,
                                       std::vector<Node*>* outOverflow) const {
    std::vector<Run> runs;

    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return runs;
    }

    // Spacing ALONG a run: children in a row are separated horizontally, children in a
    // column vertically.
    const float spacingMain = vertical ? m_SpacingY : m_SpacingX;

    // A non-positive available extent means "unconstrained": one run holding everything.
    // Breaking on `> availableMain` with availableMain <= 0 would otherwise start a new run
    // after every single child -- which is exactly what an over-padded container (padding
    // wider than the container, so a zero-extent content rect) used to do.
    const bool unconstrained = !(availableMain > 0.0f);

    Run current;

    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i];
        if (!child) {
            continue;
        }

        const Vec2 childSize = GetChildSize(child);
        const float childMain = vertical ? childSize.y : childSize.x;
        const float childCross = vertical ? childSize.x : childSize.y;

        const bool wraps = !unconstrained && !current.children.empty() &&
                           (current.mainExtent + spacingMain + childMain > availableMain);

        if (wraps) {
            runs.push_back(current);
            current = Run();

            // Cap checked HERE -- right after a run is committed, and before the child that
            // forced the break is admitted. The old code tested the cap before pushing the
            // in-progress line and then pushed it anyway on the way out of the loop, so
            // maxLines = 2 produced 3 lines.
            if (m_MaxLines > 0 && static_cast<int>(runs.size()) >= m_MaxLines) {
                if (outOverflow) {
                    outOverflow->insert(outOverflow->end(), children.begin() + i, children.end());
                }
                return runs;
            }
        }

        if (!current.children.empty()) {
            current.mainExtent += spacingMain;
        }
        current.children.push_back(child);
        current.mainExtent += childMain;
        current.crossExtent = std::max(current.crossExtent, childCross);
    }

    if (!current.children.empty()) {
        runs.push_back(current);
    }

    return runs;
}

void Wrap::ResolveRunAlignment(const Run& run, float availableMain,
                               float& outLeadingOffset, float& outSpacing) const {
    const float spacingMain = (m_WrapDirection == WrapDirection::Vertical) ? m_SpacingY : m_SpacingX;

    outLeadingOffset = 0.0f;
    outSpacing = spacingMain;

    // Slack may be negative when the run overflows (a single child wider than the
    // container cannot be broken).
    const float slack = availableMain - run.mainExtent;

    switch (m_LineAlignment) {
        case LineAlignment::Center:
            outLeadingOffset = std::max(0.0f, slack * 0.5f);
            break;

        case LineAlignment::Right:
            outLeadingOffset = std::max(0.0f, slack);
            break;

        case LineAlignment::Justify:
            // Spread the slack between the children. Clamped at zero: on an overflowing run
            // the old code produced a NEGATIVE spacing and drew the children on top of each
            // other. A single-child run has no gaps to widen and stays at the leading edge.
            if (run.children.size() > 1 && slack > 0.0f) {
                outSpacing = spacingMain + slack / static_cast<float>(run.children.size() - 1);
            }
            break;

        case LineAlignment::Left:
        default:
            break;
    }
}

void Wrap::ArrangeRun(const Run& run, bool vertical, const Rect& contentArea,
                      float runBegin, float runExtent) const {
    const float availableMain = vertical ? contentArea.size.y : contentArea.size.x;

    float leadingOffset = 0.0f;
    float spacing = 0.0f;
    ResolveRunAlignment(run, availableMain, leadingOffset, spacing);

    // The main-axis cursor advances in the direction the run flows: rightward along a row
    // (+X), but DOWNWARD along a column, which on this Y-up canvas is -Y from the top edge.
    float cursor = vertical
        ? (contentArea.GetTopEdge() - leadingOffset)
        : (contentArea.GetLeftEdge() + leadingOffset);

    for (Node* child : run.children) {
        const Vec2 childSize = GetChildSize(child);
        const float childMain = vertical ? childSize.y : childSize.x;
        const float desiredCross = vertical ? childSize.x : childSize.y;

        // Cross axis: the child's own size flags decide (Fill / ShrinkBegin / ShrinkCenter /
        // ShrinkEnd), falling back to centering within the run for children with no
        // UIControl -- which is what this container did unconditionally before.
        float crossBegin = 0.0f;
        float crossExtent = 0.0f;
        PlaceChildOnCrossAxis(child, !vertical,
                              runBegin, runExtent, desiredCross,
                              CrossAxisAlign::Center,
                              crossBegin, crossExtent);

        if (vertical) {
            // Column: cursor is the child's TOP edge, so its minimum-y corner is one child
            // height below it.
            SetChildRect(child, Rect(crossBegin, cursor - childMain, crossExtent, childMain));
            cursor -= childMain + spacing;
        } else {
            SetChildRect(child, Rect(cursor, crossBegin, childMain, crossExtent));
            cursor += childMain + spacing;
        }
    }
}

} // namespace components
} // namespace lupine
