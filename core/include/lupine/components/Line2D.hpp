#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * Represents a point on the Line2D with optional Bezier control points
 */
struct Line2DPoint {
    math::Vec2 position;           // The actual point position
    math::Vec2 controlIn;          // Control point for incoming curve (relative to position)
    math::Vec2 controlOut;         // Control point for outgoing curve (relative to position)
    bool useBezier = false;        // Whether to use Bezier curves for this point
    bool symmetricHandles = true;  // If true, controlOut = -controlIn

    Line2DPoint() : position(0, 0), controlIn(0, 0), controlOut(0, 0), useBezier(false), symmetricHandles(true) {}
    Line2DPoint(const math::Vec2& pos) : position(pos), controlIn(0, 0), controlOut(0, 0), useBezier(false), symmetricHandles(true) {}
    Line2DPoint(const math::Vec2& pos, const math::Vec2& ctrlIn, const math::Vec2& ctrlOut, bool bezier = true, bool symmetric = true)
        : position(pos), controlIn(ctrlIn), controlOut(ctrlOut), useBezier(bezier), symmetricHandles(symmetric) {}
};

/**
 * Line2D Component
 *
 * A 2D polyline component that renders lines with multiple points.
 * Can be used for drawing paths, borders, or decorative lines.
 *
 * Features:
 * - Multiple points forming a polyline
 * - Bezier curve support with control handles
 * - Customizable stroke color and width
 * - Line cap styles (butt, round, square)
 * - Line join styles (miter, round, bevel)
 * - Easy programmatic point control
 * - Closed loop option
 * - Anti-aliasing support
 * - Layer ordering
 */
class Line2D : public core::Component, public IRenderableComponent {
public:
    /**
     * Line cap styles (end of line)
     */
    enum class CapStyle {
        Butt = 0,    // Flat cap at line end
        Round = 1,   // Rounded cap
        Square = 2   // Square cap extending beyond end point
    };

    /**
     * Line join styles (corners)
     */
    enum class JoinStyle {
        Miter = 0,   // Sharp corner
        Round = 1,   // Rounded corner
        Bevel = 2    // Beveled corner
    };

    Line2D();
    explicit Line2D(const std::string& name);
    virtual ~Line2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Line2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Point Management (Legacy - positions only) =====

    /**
     * Get all point positions (without Bezier data)
     */
    std::vector<math::Vec2> GetPointPositions() const;

    /**
     * Get all points (legacy accessor, returns just positions)
     */
    const std::vector<math::Vec2>& GetPoints() const;

    /**
     * Set all points at once (simple mode, no Bezier)
     */
    void SetPoints(const std::vector<math::Vec2>& points);

    /**
     * Get point position at index
     */
    math::Vec2 GetPoint(size_t index) const;

    /**
     * Set point position at index (keeps existing Bezier settings)
     */
    void SetPoint(size_t index, const math::Vec2& point);

    /**
     * Add a point to the end (simple mode)
     */
    void AddPoint(const math::Vec2& point);

    /**
     * Insert point at index (simple mode)
     */
    void InsertPoint(size_t index, const math::Vec2& point);

    /**
     * Remove point at index
     */
    void RemovePoint(size_t index);

    /**
     * Clear all points
     */
    void ClearPoints();

    /**
     * Get number of points
     */
    size_t GetPointCount() const { return m_Line2DPoints.size(); }

    /**
     * Set beginning position (first point)
     */
    void SetBeginning(const math::Vec2& point);

    /**
     * Get beginning position (first point)
     */
    math::Vec2 GetBeginning() const;

    /**
     * Set end position (last point)
     */
    void SetEnd(const math::Vec2& point);

    /**
     * Get end position (last point)
     */
    math::Vec2 GetEnd() const;

    // ===== Bezier Point Management =====

    /**
     * Get all Line2D points with Bezier data
     */
    const std::vector<Line2DPoint>& GetLine2DPoints() const { return m_Line2DPoints; }

    /**
     * Set all Line2D points with Bezier data
     */
    void SetLine2DPoints(const std::vector<Line2DPoint>& points);

    /**
     * Get a single Line2D point with Bezier data
     */
    Line2DPoint GetLine2DPoint(size_t index) const;

    /**
     * Set a single Line2D point with Bezier data
     */
    void SetLine2DPoint(size_t index, const Line2DPoint& point);

    /**
     * Add a Line2D point with Bezier data
     */
    void AddLine2DPoint(const Line2DPoint& point);

    /**
     * Insert a Line2D point with Bezier data at index
     */
    void InsertLine2DPoint(size_t index, const Line2DPoint& point);

    /**
     * Set Bezier control handles for a point
     */
    void SetPointControlHandles(size_t index, const math::Vec2& controlIn, const math::Vec2& controlOut, bool symmetric = true);

    /**
     * Enable/disable Bezier curves for a specific point
     */
    void SetPointUseBezier(size_t index, bool useBezier);

    /**
     * Check if a point uses Bezier curves
     */
    bool GetPointUseBezier(size_t index) const;

    /**
     * Get control in handle for a point (relative to point position)
     */
    math::Vec2 GetPointControlIn(size_t index) const;

    /**
     * Get control out handle for a point (relative to point position)
     */
    math::Vec2 GetPointControlOut(size_t index) const;

    // ===== Stroke Properties =====
    
    /**
     * Get/Set stroke color
     */
    math::Color GetStrokeColor() const;
    void SetStrokeColor(const math::Color& color);
    
    /**
     * Get/Set stroke width
     */
    float GetStrokeWidth() const;
    void SetStrokeWidth(float width);
    
    /**
     * Get/Set whether the line forms a closed loop
     */
    bool GetClosedLoop() const;
    void SetClosedLoop(bool closed);

    // ===== Cap and Join Styles =====
    
    /**
     * Get/Set line cap style
     */
    CapStyle GetCapStyle() const;
    void SetCapStyle(CapStyle style);
    
    /**
     * Get/Set line join style
     */
    JoinStyle GetJoinStyle() const;
    void SetJoinStyle(JoinStyle style);

    // ===== Quality Properties =====
    
    /**
     * Get/Set anti-aliasing
     */
    bool GetAntiAliasing() const;
    void SetAntiAliasing(bool aa);
    
    /**
     * Get/Set smoothing (for round caps/joins)
     */
    int GetSmoothness() const;
    void SetSmoothness(int smoothness);

    // ===== Rendering Properties =====
    
    /**
     * Get/Set render layer
     */
    int GetLayer() const;
    void SetLayer(int layer);
    
    /**
     * Get/Set sorting order within layer
     */
    int GetSortingOrder() const;
    void SetSortingOrder(int order);
    
    /**
     * Get/Set UI space flag
     */
    bool GetUISpace() const;
    void SetUISpace(bool uiSpace);

    /**
     * Get/Set show Bezier handles (for editor visualization)
     */
    bool GetShowBezierHandles() const { return m_ShowBezierHandles; }
    void SetShowBezierHandles(bool show) { m_ShowBezierHandles = show; }

    /**
     * Get/Set selected point index for handle display (-1 means none)
     */
    int GetSelectedPointIndex() const { return m_SelectedPointIndex; }
    void SetSelectedPointIndex(int index) { m_SelectedPointIndex = index; }

    /**
     * Hit test for control points. Returns:
     * -1 if no hit
     * pointIndex * 10 + 0 for anchor point
     * pointIndex * 10 + 1 for control in
     * pointIndex * 10 + 2 for control out
     */
    int HitTestControlPoint(const math::Vec2& worldPos, float radius = 10.0f) const;

    /**
     * Move a control point by its encoded ID (from HitTestControlPoint)
     */
    void MoveControlPoint(int controlPointId, const math::Vec2& worldPos);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Internal point storage with Bezier data
    std::vector<Line2DPoint> m_Line2DPoints;

    // Cached positions for legacy API
    mutable std::vector<math::Vec2> m_PointPositionsCache;
    mutable bool m_PointsCacheDirty = true;

    void UpdatePointsCache() const;

    // Helper methods
    int CapStyleToInt(CapStyle style) const;
    CapStyle IntToCapStyle(int value) const;
    int JoinStyleToInt(JoinStyle style) const;
    JoinStyle IntToJoinStyle(int value) const;

    void SyncPointsToProperty();
    void SyncPointsFromProperty();

    void RenderLine(RenderContext& ctx, const math::Vec2& start, const math::Vec2& end);
    void RenderPolyline(RenderContext& ctx);
    void RenderBezierHandles(RenderContext& ctx);

    // Bezier curve evaluation
    math::Vec2 EvaluateCubicBezier(const math::Vec2& p0, const math::Vec2& p1,
                                    const math::Vec2& p2, const math::Vec2& p3, float t) const;

    // Generate tessellated points from Bezier curves
    std::vector<math::Vec2> TessellateCurve() const;

    // Editor mode flag for showing handles
    bool m_ShowBezierHandles = false;
    int m_SelectedPointIndex = -1;  // -1 means no point selected
};

} // namespace components
} // namespace lupine
