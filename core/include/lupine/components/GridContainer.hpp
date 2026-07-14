#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/math/Math.hpp"
#include <vector>

namespace lupine {
namespace components {

/**
 * GridContainer - Arranges child nodes in a grid layout
 *
 * Features:
 * - Configurable rows or columns
 * - Multiple cell sizing modes (automatic, fixed, hybrid)
 * - Customizable cell spacing
 * - Flow direction control
 * - Homogeneous cell sizing
 */
class GridContainer : public Container {
public:
    enum class CellSizingMode {
        Automatic = 0,           // Fit largest child in each cell
        AutoHeightFixedWidth,    // Automatic height, fixed width
        AutoWidthFixedHeight,    // Automatic width, fixed height
        Fixed                    // Fixed width and height
    };

    enum class FlowDirection {
        LeftToRight = 0,
        RightToLeft,
        TopToBottom,
        BottomToTop
    };

    GridContainer();
    explicit GridContainer(const std::string& name);
    virtual ~GridContainer();

    // ISerializable interface
    std::string GetTypeName() const override { return "GridContainer"; }
    void DefineProperties() override;

    // Lifecycle
    void OnAwake() override;
    void OnReady() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Grid configuration
    bool GetUseRows() const;
    void SetUseRows(bool useRows);

    int GetRowCount() const;
    void SetRowCount(int count);

    int GetColumnCount() const;
    void SetColumnCount(int count);

    // Cell sizing
    CellSizingMode GetCellSizingMode() const;
    void SetCellSizingMode(CellSizingMode mode);

    float GetFixedCellWidth() const;
    void SetFixedCellWidth(float width);

    float GetFixedCellHeight() const;
    void SetFixedCellHeight(float height);

    // Spacing
    float GetHorizontalSpacing() const;
    void SetHorizontalSpacing(float spacing);

    float GetVerticalSpacing() const;
    void SetVerticalSpacing(float spacing);

    // Flow direction
    FlowDirection GetFlowDirection() const;
    void SetFlowDirection(FlowDirection direction);

    // Homogeneous cells
    bool GetHomogeneousCells() const;
    void SetHomogeneousCells(bool homogeneous);

    // Child sizing
    bool GetSizeChildren() const;
    void SetSizeChildren(bool sizeChildren);
    // clipChildren is inherited from Container (GetClipChildren/SetClipChildren); it
    // drives the render traversal's ClipsDescendants()/GetClipRect() and must not be
    // shadowed here, or the grid's clip flag would disconnect from the renderer.

    // Layout override
    void CalculateLayout() override;
    math::Vec2 GetMinimumSize() const override;

protected:
    void OnLayoutInvalidated() override;

    /**
     * The grid's own content extent, so SizeMode::FitChildren sizes to the whole grid
     * rather than to the base implementation's union of child sizes (a single cell).
     */
    math::Vec2 CalculateChildrenBounds() const override;

    void SyncDerivedProperties() override;

private:
    // Grid dimensions
    void CalculateGridDimensions(int childCount, int& outRows, int& outCols) const;

    /**
     * Resolve the width of every column and the height of every row.
     *
     * A track is fixed (fixedCellWidth/Height) or measured from the children in it. When
     * homogeneousCells is set, every measured track is then raised to the largest of them,
     * so all cells match; otherwise each column is as wide as its own widest child and each
     * row as tall as its own tallest -- which is what "non-homogeneous" is supposed to mean
     * and which the old CalculateCellSize did not implement at all: it returned a hardcoded
     * 100x100 for every non-homogeneous cell.
     */
    void ComputeTracks(const std::vector<core::Node*>& childList, int rows, int cols,
                       std::vector<float>& outColumnWidths,
                       std::vector<float>& outRowHeights) const;

    /**
     * The (row, column) a child index occupies under the current flow direction.
     */
    void GetCellCoords(int index, int rows, int cols, int& outRow, int& outCol) const;

    /**
     * Total extent of the tracks plus the spacing between them (padding NOT included).
     */
    math::Vec2 GetTracksExtent(const std::vector<float>& columnWidths,
                               const std::vector<float>& rowHeights) const;

    // Grid configuration
    bool m_UseRows;
    int m_RowCount;
    int m_ColumnCount;

    // Cell sizing
    CellSizingMode m_CellSizingMode;
    float m_FixedCellWidth;
    float m_FixedCellHeight;

    // Spacing
    float m_HorizontalSpacing;
    float m_VerticalSpacing;

    // Flow
    FlowDirection m_FlowDirection;

    // Homogeneous
    bool m_HomogeneousCells;

    // Child options
    bool m_SizeChildren;
};

} // namespace components
} // namespace lupine
