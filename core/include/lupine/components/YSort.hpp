#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include <vector>
#include <unordered_set>

namespace lupine {
namespace components {

/**
 * YSort Component
 *
 * Automatically sorts the Z position of all children and grandchildren nodes
 * based on their Y (or X) position to create believable depth in 2D space.
 *
 * Features:
 * - Sorts based on Y or X axis
 * - Multiple update modes: Every Frame, On Transform Change, Manual
 * - Optional filtering to only affect sprite components
 * - Z offset customization
 * - Efficient handling of thousands of nodes
 * - Invert sort order option
 *
 * Usage:
 *   auto ySort = node->AddComponent<YSort>();
 *   ySort->SetEnabled(true);
 *   ySort->SetSortAxis(YSort::SortAxis::Y);
 *   ySort->SetUpdateMode(YSort::UpdateMode::EveryFrame);
 */
class YSort : public core::Component {
public:
    enum class SortAxis {
        Y = 0,
        X = 1
    };

    enum class UpdateMode {
        EveryFrame = 0,
        OnTransformChange = 1,
        Manual = 2
    };

    YSort();
    explicit YSort(const std::string& name);
    virtual ~YSort();

    // ISerializable interface
    std::string GetTypeName() const override { return "YSort"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnUpdate(float deltaTime) override;
    void OnLateUpdate(float deltaTime) override;

    // Property accessors
    bool GetEnabled() const;
    void SetEnabled(bool enabled);

    SortAxis GetSortAxis() const;
    void SetSortAxis(SortAxis axis);

    bool GetInvert() const;
    void SetInvert(bool invert);

    UpdateMode GetUpdateMode() const;
    void SetUpdateMode(UpdateMode mode);

    int GetZOffset() const;
    void SetZOffset(int offset);

    bool GetAffectOnlySpriteChildren() const;
    void SetAffectOnlySpriteChildren(bool spriteOnly);

    // Manual sorting trigger
    void Sort();

    // Force immediate sort and reset dirty flags
    void ForceSort();

private:
    // Internal helper structures
    struct NodeSortData {
        std::shared_ptr<core::Node> node;
        float sortValue;
        int originalZIndex; // Original Z-index for tie-breaking
        int originalDepth; // For stable sorting

        bool operator<(const NodeSortData& other) const {
            // Direct comparison - Y position is primary sort criterion
            if (sortValue != other.sortValue) {
                return sortValue < other.sortValue;
            }
            // When Y positions are exactly equal, use original Z-index as tie-breaker
            if (originalZIndex != other.originalZIndex) {
                return originalZIndex < other.originalZIndex;
            }
            // Final tie-breaker: maintain original order for equal values
            return originalDepth < other.originalDepth;
        }
    };

    // Core sorting logic
    void PerformSort();
    void GatherSortableNodes(const std::shared_ptr<core::Node>& node, std::vector<NodeSortData>& outNodes, int depth = 0);
    bool ShouldIncludeNode(const std::shared_ptr<core::Node>& node) const;
    float GetNodeSortValue(const std::shared_ptr<core::Node>& node) const;
    void ApplyZPositions(const std::vector<NodeSortData>& sortedNodes);

    // Helper to get sprite bottom offset (half height for Y, half width for X)
    float GetSpriteBottomOffset(const std::shared_ptr<core::Node>& node) const;

    // Transform change tracking
    void CheckForTransformChanges();
    void CacheNodeTransforms();

    // Cached state for change detection
    struct NodeTransformCache {
        std::weak_ptr<core::Node> node; // Use weak_ptr to avoid circular references
        math::Vec2 lastPosition;
    };
    std::vector<NodeTransformCache> m_TransformCache;

    // Dirty flag for optimization
    bool m_NeedsSorting;

    // Frame counter for throttling checks
    int m_FramesSinceLastCheck;
    static const int TRANSFORM_CHECK_INTERVAL = 3; // Check every N frames for performance
};

} // namespace components
} // namespace lupine
