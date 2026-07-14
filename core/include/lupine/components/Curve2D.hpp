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
 * Represents a point on a Curve2D with optional Bezier control points
 */
struct Curve2DPoint {
    math::Vec2 position;           // The actual point position
    math::Vec2 controlIn;          // Control point for incoming curve (relative to position)
    math::Vec2 controlOut;         // Control point for outgoing curve (relative to position)
    bool useBezier = false;        // Whether to use Bezier curves for this point
    bool symmetricHandles = true;  // If true, controlOut = -controlIn

    Curve2DPoint() : position(0, 0), controlIn(0, 0), controlOut(0, 0), useBezier(false), symmetricHandles(true) {}
    Curve2DPoint(const math::Vec2& pos) : position(pos), controlIn(0, 0), controlOut(0, 0), useBezier(false), symmetricHandles(true) {}
    Curve2DPoint(const math::Vec2& pos, const math::Vec2& ctrlIn, const math::Vec2& ctrlOut, bool bezier = true, bool symmetric = true)
        : position(pos), controlIn(ctrlIn), controlOut(ctrlOut), useBezier(bezier), symmetricHandles(symmetric) {}
};

/**
 * Curve2D Component
 *
 * A 2D curve data component that defines a curve path without rendering.
 * Provides curve data for pathing, collision, or other uses.
 * Has toggleable debug rendering for easy editing in the editor.
 *
 * Features:
 * - Multiple points forming a curve
 * - Bezier curve support with control handles
 * - Toggleable debug visualization
 * - Curve tessellation for sampling
 * - Closed loop option
 * - Easy programmatic point control
 */
class Curve2D : public core::Component, public IRenderableComponent {
public:
    Curve2D();
    explicit Curve2D(const std::string& name);
    virtual ~Curve2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Curve2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Point Management =====

    /** Get all curve points */
    const std::vector<Curve2DPoint>& GetCurvePoints() const { return m_CurvePoints; }

    /** Set all curve points */
    void SetCurvePoints(const std::vector<Curve2DPoint>& points);

    /** Get a single point */
    Curve2DPoint GetCurvePoint(size_t index) const;

    /** Set a single point */
    void SetCurvePoint(size_t index, const Curve2DPoint& point);

    /** Add a point to the end */
    void AddCurvePoint(const Curve2DPoint& point);

    /** Add a point by position (simple mode) */
    void AddPoint(const math::Vec2& point);

    /** Insert point at index */
    void InsertCurvePoint(size_t index, const Curve2DPoint& point);

    /** Remove point at index */
    void RemovePoint(size_t index);

    /** Clear all points */
    void ClearPoints();

    /** Get number of points */
    size_t GetPointCount() const { return m_CurvePoints.size(); }

    /** Get point position at index */
    math::Vec2 GetPointPosition(size_t index) const;

    /** Set point position at index */
    void SetPointPosition(size_t index, const math::Vec2& position);

    // ===== Bezier Control =====

    /** Set Bezier control handles for a point */
    void SetPointControlHandles(size_t index, const math::Vec2& controlIn, const math::Vec2& controlOut, bool symmetric = true);

    /** Enable/disable Bezier for a point */
    void SetPointUseBezier(size_t index, bool useBezier);

    /** Check if point uses Bezier */
    bool GetPointUseBezier(size_t index) const;

    /** Get control in handle */
    math::Vec2 GetPointControlIn(size_t index) const;

    /** Get control out handle */
    math::Vec2 GetPointControlOut(size_t index) const;

    // ===== Curve Properties =====

    /** Get/Set closed loop */
    bool GetClosedLoop() const;
    void SetClosedLoop(bool closed);

    /** Get/Set debug draw enabled */
    bool GetDebugDraw() const;
    void SetDebugDraw(bool enabled);

    /** Get/Set debug color */
    math::Color GetDebugColor() const;
    void SetDebugColor(const math::Color& color);

    /** Get/Set tessellation segments per bezier curve */
    int GetBezierSegments() const;
    void SetBezierSegments(int segments);

    // ===== Curve Sampling =====

    /** Get tessellated points (for rendering or collision) */
    std::vector<math::Vec2> GetTessellatedPoints() const;

    /** Get total curve length */
    float GetCurveLength() const;

    /** Sample a point on the curve at t (0-1) */
    math::Vec2 SampleCurve(float t) const;

    /** Sample direction/tangent at t (0-1) */
    math::Vec2 SampleTangent(float t) const;

    // ===== Editor Support =====

    bool GetShowBezierHandles() const { return m_ShowBezierHandles; }
    void SetShowBezierHandles(bool show) { m_ShowBezierHandles = show; }

    int GetSelectedPointIndex() const { return m_SelectedPointIndex; }
    void SetSelectedPointIndex(int index) { m_SelectedPointIndex = index; }

    int HitTestControlPoint(const math::Vec2& worldPos, float radius = 10.0f) const;
    void MoveControlPoint(int controlPointId, const math::Vec2& worldPos);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

protected:
    std::vector<Curve2DPoint> m_CurvePoints;
    mutable bool m_CacheDirty = true;
    mutable std::vector<math::Vec2> m_TessellatedCache;
    mutable float m_CachedLength = 0.0f;

    void InvalidateCache() { m_CacheDirty = true; }
    void UpdateCache() const;

    void SyncPointsToProperty();
    void SyncPointsFromProperty();

    math::Vec2 EvaluateCubicBezier(const math::Vec2& p0, const math::Vec2& p1,
                                    const math::Vec2& p2, const math::Vec2& p3, float t) const;

    void RenderDebugCurve(RenderContext& ctx);
    void RenderBezierHandles(RenderContext& ctx);

    bool m_ShowBezierHandles = false;
    int m_SelectedPointIndex = -1;
};

} // namespace components
} // namespace lupine

