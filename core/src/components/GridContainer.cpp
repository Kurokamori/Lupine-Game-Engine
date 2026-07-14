#include "lupine/components/GridContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

GridContainer::GridContainer()
    : Container("GridContainer")
    , m_UseRows(false)
    , m_RowCount(2)
    , m_ColumnCount(2)
    , m_CellSizingMode(CellSizingMode::Automatic)
    , m_FixedCellWidth(100.0f)
    , m_FixedCellHeight(100.0f)
    , m_HorizontalSpacing(5.0f)
    , m_VerticalSpacing(5.0f)
    , m_FlowDirection(FlowDirection::LeftToRight)
    , m_HomogeneousCells(true)
    , m_SizeChildren(false)
{
}

GridContainer::GridContainer(const std::string& name)
    : Container(name)
    , m_UseRows(false)
    , m_RowCount(2)
    , m_ColumnCount(2)
    , m_CellSizingMode(CellSizingMode::Automatic)
    , m_FixedCellWidth(100.0f)
    , m_FixedCellHeight(100.0f)
    , m_HorizontalSpacing(5.0f)
    , m_VerticalSpacing(5.0f)
    , m_FlowDirection(FlowDirection::LeftToRight)
    , m_HomogeneousCells(true)
    , m_SizeChildren(false)
{
}

GridContainer::~GridContainer() {
}

void GridContainer::DefineProperties() {
    // Call base class properties
    Container::DefineProperties();

    // Grid Configuration
    DefineProperty(PROPERTY_DEFAULT_GROUP(useRows, Bool, false, "Grid"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(rowCount, 2, 1, 100, 1, "Grid"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(columnCount, 2, 1, 100, 1, "Grid"));

    // Cell Sizing
    DefineProperty(PROPERTY_ENUM_GROUP(cellSizingMode, 0, "Cell Sizing",
        Automatic, AutoHeightFixedWidth, AutoWidthFixedHeight, Fixed));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fixedCellWidth, 100.0f, 1.0f, 10000.0f, 1.0f, "Cell Sizing"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fixedCellHeight, 100.0f, 1.0f, 10000.0f, 1.0f, "Cell Sizing"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(homogeneousCells, Bool, true, "Cell Sizing"));

    // Spacing
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(horizontalSpacing, 5.0f, 0.0f, 1000.0f, 1.0f, "Spacing"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(verticalSpacing, 5.0f, 0.0f, 1000.0f, 1.0f, "Spacing"));

    // Flow Direction
    DefineProperty(PROPERTY_ENUM_GROUP(flowDirection, 0, "Flow",
        LeftToRight, RightToLeft, TopToBottom, BottomToTop));

    // Child Options
    DefineProperty(PROPERTY_DEFAULT_GROUP(sizeChildren, Bool, false, "Child Options"));
    // Note: clipChildren is defined and handled by the base Container (it drives the
    // render traversal's ClipsDescendants()/GetClipRect()); a GridContainer-local copy
    // would shadow and disconnect it, so it is intentionally not redefined here.
}

void GridContainer::OnAwake() {
    Container::OnAwake();

    SyncDerivedProperties();
    InvalidateLayout();
}

void GridContainer::OnReady() {
    Container::OnReady();
    InvalidateLayout();
}

void GridContainer::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    // The base runs SyncFromProperties() -> SyncDerivedProperties(), which refreshes every
    // member below. This used to be a parallel if/else chain here AND a second verbatim
    // copy at the top of CalculateLayout().
    Container::OnPropertyChanged(propertyName, newValue);
}

void GridContainer::SyncDerivedProperties() {
    SyncCachedBool("useRows", m_UseRows);
    SyncCachedInt("rowCount", m_RowCount);
    SyncCachedInt("columnCount", m_ColumnCount);
    SyncCachedEnum("cellSizingMode", m_CellSizingMode);
    SyncCachedFloat("fixedCellWidth", m_FixedCellWidth);
    SyncCachedFloat("fixedCellHeight", m_FixedCellHeight);
    SyncCachedFloat("horizontalSpacing", m_HorizontalSpacing);
    SyncCachedFloat("verticalSpacing", m_VerticalSpacing);
    SyncCachedEnum("flowDirection", m_FlowDirection);
    SyncCachedBool("homogeneousCells", m_HomogeneousCells);
    SyncCachedBool("sizeChildren", m_SizeChildren);

    m_RowCount = std::max(1, m_RowCount);
    m_ColumnCount = std::max(1, m_ColumnCount);
}

// ========================================
// Grid Configuration
// ========================================

bool GridContainer::GetUseRows() const {
    return m_UseRows;
}

void GridContainer::SetUseRows(bool useRows) {
    m_UseRows = useRows;
    SetPropertyValue<bool>("useRows", useRows);
    InvalidateLayout();
}

int GridContainer::GetRowCount() const {
    return m_RowCount;
}

void GridContainer::SetRowCount(int count) {
    m_RowCount = std::max(1, count);
    SetPropertyValue<int>("rowCount", m_RowCount);
    InvalidateLayout();
}

int GridContainer::GetColumnCount() const {
    return m_ColumnCount;
}

void GridContainer::SetColumnCount(int count) {
    m_ColumnCount = std::max(1, count);
    SetPropertyValue<int>("columnCount", m_ColumnCount);
    InvalidateLayout();
}

// ========================================
// Cell Sizing
// ========================================

GridContainer::CellSizingMode GridContainer::GetCellSizingMode() const {
    return m_CellSizingMode;
}

void GridContainer::SetCellSizingMode(CellSizingMode mode) {
    m_CellSizingMode = mode;
    SetPropertyValue<int>("cellSizingMode", static_cast<int>(mode));
    InvalidateLayout();
}

float GridContainer::GetFixedCellWidth() const {
    return m_FixedCellWidth;
}

void GridContainer::SetFixedCellWidth(float width) {
    m_FixedCellWidth = std::max(1.0f, width);
    SetPropertyValue<float>("fixedCellWidth", m_FixedCellWidth);
    InvalidateLayout();
}

float GridContainer::GetFixedCellHeight() const {
    return m_FixedCellHeight;
}

void GridContainer::SetFixedCellHeight(float height) {
    m_FixedCellHeight = std::max(1.0f, height);
    SetPropertyValue<float>("fixedCellHeight", m_FixedCellHeight);
    InvalidateLayout();
}

// ========================================
// Spacing
// ========================================

float GridContainer::GetHorizontalSpacing() const {
    return m_HorizontalSpacing;
}

void GridContainer::SetHorizontalSpacing(float spacing) {
    m_HorizontalSpacing = std::max(0.0f, spacing);
    SetPropertyValue<float>("horizontalSpacing", m_HorizontalSpacing);
    InvalidateLayout();
}

float GridContainer::GetVerticalSpacing() const {
    return m_VerticalSpacing;
}

void GridContainer::SetVerticalSpacing(float spacing) {
    m_VerticalSpacing = std::max(0.0f, spacing);
    SetPropertyValue<float>("verticalSpacing", m_VerticalSpacing);
    InvalidateLayout();
}

// ========================================
// Flow Direction
// ========================================

GridContainer::FlowDirection GridContainer::GetFlowDirection() const {
    return m_FlowDirection;
}

void GridContainer::SetFlowDirection(FlowDirection direction) {
    m_FlowDirection = direction;
    SetPropertyValue<int>("flowDirection", static_cast<int>(direction));
    InvalidateLayout();
}

// ========================================
// Homogeneous Cells
// ========================================

bool GridContainer::GetHomogeneousCells() const {
    return m_HomogeneousCells;
}

void GridContainer::SetHomogeneousCells(bool homogeneous) {
    m_HomogeneousCells = homogeneous;
    SetPropertyValue<bool>("homogeneousCells", homogeneous);
    InvalidateLayout();
}

// ========================================
// Child Options
// ========================================

bool GridContainer::GetSizeChildren() const {
    return m_SizeChildren;
}

void GridContainer::SetSizeChildren(bool sizeChildren) {
    m_SizeChildren = sizeChildren;
    SetPropertyValue<bool>("sizeChildren", sizeChildren);
    InvalidateLayout();
}

// ========================================
// Layout Implementation
// ========================================


void GridContainer::CalculateLayout() {
    if (!m_Owner) {
        return;
    }

    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return;
    }

    int rows = 0;
    int cols = 0;
    CalculateGridDimensions(static_cast<int>(children.size()), rows, cols);

    std::vector<float> columnWidths;
    std::vector<float> rowHeights;
    ComputeTracks(children, rows, cols, columnWidths, rowHeights);

    const Rect contentArea = GetContentRect();
    const float left = contentArea.GetLeftEdge();
    const float top = contentArea.GetTopEdge();

    // Track offsets. columnOffsets[c] is the distance RIGHT of the content area's left
    // edge; rowOffsets[r] is the distance DOWN from its top edge, so it is SUBTRACTED from
    // the top edge on this Y-up canvas, never added.
    std::vector<float> columnOffsets(columnWidths.size(), 0.0f);
    float cursor = 0.0f;
    for (size_t c = 0; c < columnWidths.size(); ++c) {
        columnOffsets[c] = cursor;
        cursor += columnWidths[c] + m_HorizontalSpacing;
    }

    std::vector<float> rowOffsets(rowHeights.size(), 0.0f);
    cursor = 0.0f;
    for (size_t r = 0; r < rowHeights.size(); ++r) {
        rowOffsets[r] = cursor;
        cursor += rowHeights[r] + m_VerticalSpacing;
    }

    for (size_t i = 0; i < children.size(); ++i) {
        Node* child = children[i];
        if (!child) {
            continue;
        }

        int row = 0;
        int col = 0;
        GetCellCoords(static_cast<int>(i), rows, cols, row, col);
        if (row < 0 || col < 0 ||
            row >= static_cast<int>(rowHeights.size()) ||
            col >= static_cast<int>(columnWidths.size())) {
            continue;
        }

        const Rect cell = Rect::FromEdges(
            left + columnOffsets[col],
            top - rowOffsets[row],
            left + columnOffsets[col] + columnWidths[col],
            top - rowOffsets[row] - rowHeights[row]
        );

        if (m_SizeChildren) {
            // sizeChildren is the container-wide "every child is exactly its cell" switch,
            // and it deliberately overrides the per-child size flags.
            SetChildRect(child, cell);
            continue;
        }

        // Otherwise the child's own size flags decide how it sits in its cell, on BOTH axes.
        // The fallback for a child with no UIControl is the cell's top-left corner, which is
        // where the old implementation pinned every child unconditionally.
        const Vec2 desired = GetChildSize(child);

        float childX = 0.0f;
        float childWidth = 0.0f;
        PlaceChildOnCrossAxis(child, false,
                              cell.GetLeftEdge(), cell.size.x, desired.x,
                              CrossAxisAlign::Begin,
                              childX, childWidth);

        float childY = 0.0f;
        float childHeight = 0.0f;
        PlaceChildOnCrossAxis(child, true,
                              cell.GetBottomEdge(), cell.size.y, desired.y,
                              CrossAxisAlign::End,
                              childY, childHeight);

        SetChildRect(child, Rect(childX, childY, childWidth, childHeight));
    }

    // NOTE: the container's own size is NOT written here. This used to call SetWidth() /
    // SetHeight() in FitChildren mode, which raised m_LayoutDirty from inside
    // CalculateLayout() -- an invalidation the caller then cleared away underneath it -- and
    // was useless anyway, because Container::GetWidth() ignores the width property whenever
    // the size mode is not Fixed. FitChildren now reads CalculateChildrenBounds() below.
}

math::Vec2 GridContainer::CalculateChildrenBounds() const {
    const std::vector<Node*> children = GetVisibleChildren();
    if (children.empty()) {
        return Vec2(0.0f, 0.0f);
    }

    int rows = 0;
    int cols = 0;
    CalculateGridDimensions(static_cast<int>(children.size()), rows, cols);

    std::vector<float> columnWidths;
    std::vector<float> rowHeights;
    ComputeTracks(children, rows, cols, columnWidths, rowHeights);

    return GetTracksExtent(columnWidths, rowHeights);
}

math::Vec2 GridContainer::GetMinimumSize() const {
    // The authored customMinSize is a FLOOR. The old implementation dropped it entirely as
    // soon as the grid had any children -- it returned the cell math with no max() against
    // the base -- so a grid with a customMinSize could be allocated less than it asked for.
    const Vec2 floorSize = Container::GetMinimumSize();

    const Vec2 content = CalculateChildrenBounds();

    const float padX = GetPaddingLeft() + GetPaddingRight();
    const float padY = GetPaddingTop() + GetPaddingBottom();

    return Vec2(
        std::max(floorSize.x, content.x + padX),
        std::max(floorSize.y, content.y + padY)
    );
}

void GridContainer::OnLayoutInvalidated() {
    Container::OnLayoutInvalidated();
}

// ========================================
// Private Helper Methods
// ========================================

void GridContainer::CalculateGridDimensions(int childCount, int& outRows, int& outCols) const {
    if (childCount <= 0) {
        outRows = 0;
        outCols = 0;
        return;
    }

    if (m_UseRows) {
        // User specifies rows, calculate columns
        outRows = std::max(1, m_RowCount);
        outCols = static_cast<int>(std::ceil(static_cast<float>(childCount) / static_cast<float>(outRows)));
    } else {
        // User specifies columns, calculate rows
        outCols = std::max(1, m_ColumnCount);
        outRows = static_cast<int>(std::ceil(static_cast<float>(childCount) / static_cast<float>(outCols)));
    }

    outRows = std::max(1, outRows);
    outCols = std::max(1, outCols);
}

void GridContainer::ComputeTracks(const std::vector<core::Node*>& childList, int rows, int cols,
                                  std::vector<float>& outColumnWidths,
                                  std::vector<float>& outRowHeights) const {
    outColumnWidths.assign(static_cast<size_t>(std::max(0, cols)), 0.0f);
    outRowHeights.assign(static_cast<size_t>(std::max(0, rows)), 0.0f);

    if (outColumnWidths.empty() || outRowHeights.empty()) {
        return;
    }

    const bool widthIsFixed = (m_CellSizingMode == CellSizingMode::Fixed ||
                               m_CellSizingMode == CellSizingMode::AutoHeightFixedWidth);
    const bool heightIsFixed = (m_CellSizingMode == CellSizingMode::Fixed ||
                                m_CellSizingMode == CellSizingMode::AutoWidthFixedHeight);

    if (widthIsFixed) {
        std::fill(outColumnWidths.begin(), outColumnWidths.end(), m_FixedCellWidth);
    }
    if (heightIsFixed) {
        std::fill(outRowHeights.begin(), outRowHeights.end(), m_FixedCellHeight);
    }
    if (widthIsFixed && heightIsFixed) {
        return;
    }

    // Measure each track against the children that actually land in it.
    for (size_t i = 0; i < childList.size(); ++i) {
        core::Node* child = childList[i];
        if (!child) {
            continue;
        }

        int row = 0;
        int col = 0;
        GetCellCoords(static_cast<int>(i), rows, cols, row, col);
        if (row < 0 || col < 0 ||
            row >= static_cast<int>(outRowHeights.size()) ||
            col >= static_cast<int>(outColumnWidths.size())) {
            continue;
        }

        const Vec2 childSize = GetChildSize(child);
        if (!widthIsFixed) {
            outColumnWidths[col] = std::max(outColumnWidths[col], childSize.x);
        }
        if (!heightIsFixed) {
            outRowHeights[row] = std::max(outRowHeights[row], childSize.y);
        }
    }

    // Homogeneous: every measured track is raised to the largest of them, so all cells match.
    // Non-homogeneous keeps each column at its own widest child and each row at its own
    // tallest -- which is what the mode means, and which the old code did not implement at
    // all: it returned a hardcoded 100x100 for every non-homogeneous cell.
    if (m_HomogeneousCells) {
        if (!widthIsFixed) {
            const float widest = *std::max_element(outColumnWidths.begin(), outColumnWidths.end());
            std::fill(outColumnWidths.begin(), outColumnWidths.end(), widest);
        }
        if (!heightIsFixed) {
            const float tallest = *std::max_element(outRowHeights.begin(), outRowHeights.end());
            std::fill(outRowHeights.begin(), outRowHeights.end(), tallest);
        }
    }
}

math::Vec2 GridContainer::GetTracksExtent(const std::vector<float>& columnWidths,
                                          const std::vector<float>& rowHeights) const {
    Vec2 extent(0.0f, 0.0f);

    for (float w : columnWidths) {
        extent.x += w;
    }
    for (float h : rowHeights) {
        extent.y += h;
    }

    if (columnWidths.size() > 1) {
        extent.x += m_HorizontalSpacing * static_cast<float>(columnWidths.size() - 1);
    }
    if (rowHeights.size() > 1) {
        extent.y += m_VerticalSpacing * static_cast<float>(rowHeights.size() - 1);
    }

    return extent;
}

void GridContainer::GetCellCoords(int index, int rows, int cols, int& outRow, int& outCol) const {
    outRow = 0;
    outCol = 0;

    if (rows <= 0 || cols <= 0) {
        return;
    }

    switch (m_FlowDirection) {
        case FlowDirection::LeftToRight:
            outRow = index / cols;
            outCol = index % cols;
            break;

        case FlowDirection::RightToLeft:
            outRow = index / cols;
            outCol = cols - 1 - (index % cols);
            break;

        case FlowDirection::TopToBottom:
            outCol = index / rows;
            outRow = index % rows;
            break;

        case FlowDirection::BottomToTop:
            outCol = index / rows;
            outRow = rows - 1 - (index % rows);
            break;
    }
}

} // namespace components
} // namespace lupine
