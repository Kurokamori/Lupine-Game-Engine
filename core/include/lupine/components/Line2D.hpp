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
 * Line2D Component
 *
 * A 2D polyline component that renders lines with multiple points.
 * Can be used for drawing paths, borders, or decorative lines.
 *
 * Features:
 * - Multiple points forming a polyline
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

    // ===== Point Management =====
    
    /**
     * Get all points
     */
    const std::vector<math::Vec2>& GetPoints() const { return m_Points; }
    
    /**
     * Set all points at once
     */
    void SetPoints(const std::vector<math::Vec2>& points);
    
    /**
     * Get point at index
     */
    math::Vec2 GetPoint(size_t index) const;
    
    /**
     * Set point at index
     */
    void SetPoint(size_t index, const math::Vec2& point);
    
    /**
     * Add a point to the end
     */
    void AddPoint(const math::Vec2& point);
    
    /**
     * Insert point at index
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
    size_t GetPointCount() const { return m_Points.size(); }
    
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

    // ===== Stroke Properties =====
    
    /**
     * Get/Set stroke color
     */
    const math::Color& GetStrokeColor() const;
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

    // ===== IRenderableComponent Implementation =====
    
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Internal point storage
    std::vector<math::Vec2> m_Points;
    
    // Helper methods
    int CapStyleToInt(CapStyle style) const;
    CapStyle IntToCapStyle(int value) const;
    int JoinStyleToInt(JoinStyle style) const;
    JoinStyle IntToJoinStyle(int value) const;
    
    void SyncPointsToProperty();
    void SyncPointsFromProperty();
    
    void RenderLine(RenderContext& ctx, const math::Vec2& start, const math::Vec2& end);
    void RenderPolyline(RenderContext& ctx);
};

} // namespace components
} // namespace lupine
