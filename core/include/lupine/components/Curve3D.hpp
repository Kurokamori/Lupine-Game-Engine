#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * Represents a point on a Curve3D with optional Bezier control points.
 * All vectors are expressed in the owning node's local space; control
 * handles are relative to the point position.
 */
struct Curve3DPoint {
    math::Vec3 position;           // The actual point position (local space)
    math::Vec3 controlIn;          // Control handle for the incoming curve (relative to position)
    math::Vec3 controlOut;         // Control handle for the outgoing curve (relative to position)
    bool useBezier = false;        // Whether to use Bezier curves for this point
    bool symmetricHandles = true;  // If true, controlOut = -controlIn
    float tilt = 0.0f;             // Roll about the tangent (radians); interpolated along the curve

    Curve3DPoint() : position(0, 0, 0), controlIn(0, 0, 0), controlOut(0, 0, 0), useBezier(false), symmetricHandles(true), tilt(0.0f) {}
    explicit Curve3DPoint(const math::Vec3& pos) : position(pos), controlIn(0, 0, 0), controlOut(0, 0, 0), useBezier(false), symmetricHandles(true), tilt(0.0f) {}
    Curve3DPoint(const math::Vec3& pos, const math::Vec3& ctrlIn, const math::Vec3& ctrlOut, bool bezier = true, bool symmetric = true)
        : position(pos), controlIn(ctrlIn), controlOut(ctrlOut), useBezier(bezier), symmetricHandles(symmetric), tilt(0.0f) {}
};

/**
 * Curve3D Component
 *
 * A 3D curve data component that defines a curve path without rendering geometry.
 * Provides curve data for camera rails, moving platforms, cutscene tracks, and
 * spline-based AI movement. Has toggleable debug line rendering for the viewport.
 *
 * Points are stored in the owning Node3D's local space. Debug rendering and the
 * world-space query helpers transform points through the node's global transform
 * matrix, so the curve respects the node's rotation and scale (unlike the 2D
 * variant which only offsets by global position).
 *
 * Features:
 * - Multiple points forming a curve
 * - Cubic Bezier curve support with control handles
 * - Toggleable debug visualization (3D lines)
 * - Curve tessellation for sampling
 * - Closed loop option
 * - Arc-length based sampling (uniform speed along the curve)
 * - Easy programmatic point control
 * - Script-accessible via CallMethod (all scripting languages)
 */
class Curve3D : public core::Component, public IRenderableComponent {
public:
    Curve3D();
    explicit Curve3D(const std::string& name);
    virtual ~Curve3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Curve3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Scripting bridge (shared by Lua / MRuby / MicroPython / C-API)
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Point Management =====

    /** Get all curve points */
    const std::vector<Curve3DPoint>& GetCurvePoints() const { return m_CurvePoints; }

    /** Set all curve points */
    void SetCurvePoints(const std::vector<Curve3DPoint>& points);

    /** Get a single point */
    Curve3DPoint GetCurvePoint(size_t index) const;

    /** Set a single point */
    void SetCurvePoint(size_t index, const Curve3DPoint& point);

    /** Add a point to the end */
    void AddCurvePoint(const Curve3DPoint& point);

    /** Add a point by position (simple mode) */
    void AddPoint(const math::Vec3& point);

    /** Insert point at index */
    void InsertCurvePoint(size_t index, const Curve3DPoint& point);

    /** Remove point at index */
    void RemovePoint(size_t index);

    /** Clear all points */
    void ClearPoints();

    /** Get number of points */
    size_t GetPointCount() const { return m_CurvePoints.size(); }

    /** Get point position at index */
    math::Vec3 GetPointPosition(size_t index) const;

    /** Set point position at index */
    void SetPointPosition(size_t index, const math::Vec3& position);

    // ===== Bezier Control =====

    /** Set Bezier control handles for a point */
    void SetPointControlHandles(size_t index, const math::Vec3& controlIn, const math::Vec3& controlOut, bool symmetric = true);

    /** Enable/disable Bezier for a point */
    void SetPointUseBezier(size_t index, bool useBezier);

    /** Check if point uses Bezier */
    bool GetPointUseBezier(size_t index) const;

    /** Get control in handle */
    math::Vec3 GetPointControlIn(size_t index) const;

    /** Get control out handle */
    math::Vec3 GetPointControlOut(size_t index) const;

    /** Get/Set per-point tilt (roll about the tangent, in radians) */
    float GetPointTilt(size_t index) const;
    void SetPointTilt(size_t index, float tilt);

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

    // ===== Curve Sampling (local space) =====

    /** Get tessellated points in local space (for rendering or collision) */
    std::vector<math::Vec3> GetTessellatedPoints() const;

    /** Get total curve length (in local-space units) */
    float GetCurveLength() const;

    /** Sample a point on the curve at t (0-1), arc-length parameterized, local space */
    math::Vec3 SampleCurve(float t) const;

    /** Sample direction/tangent at t (0-1), normalized, local space */
    math::Vec3 SampleTangent(float t) const;

    /** Sample interpolated tilt (roll about the tangent, radians) at t (0-1) */
    float SampleTilt(float t) const;

    /** Sample the up vector at t (0-1), local space (tangent frame rolled by tilt) */
    math::Vec3 SampleUpVector(float t) const;

    /** Sample a full orientation at t (0-1) as a quaternion, local space.
     *  Forward (-Z) follows the tangent; up follows the rolled tangent frame. */
    math::Quat SampleOrientation(float t) const;

    // ===== World-space helpers (transform through owner Node3D) =====

    /** Transform a local-space point into world space using the owner transform */
    math::Vec3 LocalToWorld(const math::Vec3& localPoint) const;

    /** Transform a local-space direction into world space (rotation/scale only) */
    math::Vec3 LocalDirectionToWorld(const math::Vec3& localDir) const;

    /** Sample a world-space point on the curve at t (0-1) */
    math::Vec3 SampleCurveWorld(float t) const;

    /** Sample a world-space tangent at t (0-1) */
    math::Vec3 SampleTangentWorld(float t) const;

    /** Sample a world-space up vector at t (0-1) */
    math::Vec3 SampleUpVectorWorld(float t) const;

    /** Sample a world-space orientation at t (0-1) */
    math::Quat SampleOrientationWorld(float t) const;

    // ===== Editor Support =====

    bool GetShowBezierHandles() const { return m_ShowBezierHandles; }
    void SetShowBezierHandles(bool show) { m_ShowBezierHandles = show; }

    int GetSelectedPointIndex() const { return m_SelectedPointIndex; }
    void SetSelectedPointIndex(int index) { m_SelectedPointIndex = index; }

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

protected:
    std::vector<Curve3DPoint> m_CurvePoints;
    mutable bool m_CacheDirty = true;
    mutable std::vector<math::Vec3> m_TessellatedCache;
    mutable std::vector<float> m_TessellatedTilt;
    mutable float m_CachedLength = 0.0f;

    void InvalidateCache() { m_CacheDirty = true; }
    void UpdateCache() const;

    void SyncPointsToProperty();
    void SyncPointsFromProperty();

    math::Mat4 GetOwnerWorldMatrix() const;

    math::Vec3 EvaluateCubicBezier(const math::Vec3& p0, const math::Vec3& p1,
                                   const math::Vec3& p2, const math::Vec3& p3, float t) const;

    void RenderDebugCurve(RenderContext& ctx);
    void RenderEditorHandles(RenderContext& ctx);

    bool m_ShowBezierHandles = false;
    int m_SelectedPointIndex = -1;
};

} // namespace components
} // namespace lupine
