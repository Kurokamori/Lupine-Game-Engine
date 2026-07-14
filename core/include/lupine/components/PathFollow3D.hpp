#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace lupine {
namespace components {

class Curve3D;

/**
 * PathFollow3D Component
 *
 * Drives its owning Node3D's transform along a Curve3D/Path3D — for moving
 * platforms, camera rails, follow cameras, patrolling enemies, and cutscene
 * tracks.
 *
 * The target path is located by the `pathNode` property (a NodePath to the node
 * holding the Curve3D/Path3D). If empty, the owner's parent node is used (the
 * common case: the follower is a child of the path node).
 *
 * Each PathFollow3D keeps its own progress, so many followers can ride the same
 * path at different positions/speeds. Position and orientation are sampled in
 * world space and converted into the owner's local transform, so the follower
 * does not have to be a direct child of the path node.
 *
 * Editor live-preview: while a `previewInEditor` is on, scrubbing `progressRatio`
 * in the inspector snaps the owner along the path in the viewport.
 */
class PathFollow3D : public core::Component, public IRenderableComponent {
public:
    enum class RotationMode {
        None = 0,    // Do not change the owner's rotation
        Forward = 1, // Full orientation: forward (-Z) along the tangent, up from the curve tilt frame
        YawOnly = 2  // Yaw only (keep the owner upright)
    };

    PathFollow3D();
    explicit PathFollow3D(const std::string& name);
    virtual ~PathFollow3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "PathFollow3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    // Scripting bridge
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Following control =====

    void StartFollowing();
    void StopFollowing();
    bool IsFollowing() const { return m_Playing; }
    void Reset();

    /** Progress along the path as a 0-1 ratio (primary position knob) */
    float GetProgressRatio() const;
    void SetProgressRatio(float ratio);

    /** Progress along the path as an absolute distance (units) */
    float GetProgressDistance() const;
    void SetProgressDistance(float distance);

    // ===== Property accessors =====

    std::string GetPathNodePath() const;
    void SetPathNodePath(const std::string& path);

    float GetOffset() const;
    void SetOffset(float offset);

    float GetHOffset() const;
    void SetHOffset(float h);

    float GetVOffset() const;
    void SetVOffset(float v);

    RotationMode GetRotationMode() const;
    void SetRotationMode(RotationMode mode);

    bool GetFlipForward() const;
    void SetFlipForward(bool flip);

    float GetSpeed() const;
    void SetSpeed(float speed);

    bool GetLoop() const;
    void SetLoop(bool loop);

    bool GetPingPong() const;
    void SetPingPong(bool pingPong);

    bool GetAutoStart() const;
    void SetAutoStart(bool autoStart);

    // ===== Resolution / application =====

    /** Resolve the target curve/path component (pathNode, else parent node, else self) */
    Curve3D* ResolvePath() const;

    /** Sample the path at the current progress and write the owner's transform */
    void ApplyToOwner();

    // ===== IRenderableComponent (editor preview + marker) =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    void SetProgressRatioRaw(float ratio);
    float NormalizeRatio(float ratio) const;

    bool m_Playing = false;
    int m_Direction = 1;
};

} // namespace components
} // namespace lupine
