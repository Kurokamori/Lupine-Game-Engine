#include "lupine/components/YSort.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Shape2D.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <memory>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

YSort::YSort()
    : Component("YSort")
    , m_NeedsSorting(true)
    , m_FramesSinceLastCheck(0)
{
}

YSort::YSort(const std::string& name)
    : Component(name)
    , m_NeedsSorting(true)
    , m_FramesSinceLastCheck(0)
{
}

YSort::~YSort() {
}

void YSort::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(
        enabled, Bool, true, "YSort"
    ));

    DefineProperty(PROPERTY_ENUM_GROUP(
        sortAxis, 0, "YSort",
        Y, X
    ));

    DefineProperty(PROPERTY_DEFAULT_GROUP(
        invert, Bool, false, "YSort"
    ));

    DefineProperty(PROPERTY_ENUM_GROUP(
        updateMode, 0, "YSort",
        EveryFrame, OnTransformChange, Manual
    ));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(
        zOffset, 0, -10000, 10000, 1, "YSort"
    ));

    DefineProperty(PROPERTY_DEFAULT_GROUP(
        affectOnlySpriteChildren, Bool, false, "YSort"
    ));
}

void YSort::OnAwake() {
    // Pre-allocate space for common cases
    m_TransformCache.reserve(128);
    m_NeedsSorting = true;
}

void YSort::OnUpdate(float) {
    if (!GetEnabled()) {
        return;
    }

    UpdateMode mode = GetUpdateMode();

    // Check for changes in OnUpdate, but defer sorting to LateUpdate
    if (mode == UpdateMode::OnTransformChange) {
        // Throttle transform checks for performance
        m_FramesSinceLastCheck++;
        if (m_FramesSinceLastCheck >= TRANSFORM_CHECK_INTERVAL) {
            CheckForTransformChanges();
            m_FramesSinceLastCheck = 0;
        }
    }
    else if (mode == UpdateMode::EveryFrame) {
        // Mark as needing sort for LateUpdate
        m_NeedsSorting = true;
    }
    // Manual mode: sorting only happens when Sort() is called explicitly
}

void YSort::OnLateUpdate(float) {
    if (!GetEnabled()) {
        return;
    }

    UpdateMode mode = GetUpdateMode();

    // Perform sorting in LateUpdate to ensure all position updates have completed
    if (mode == UpdateMode::EveryFrame || mode == UpdateMode::OnTransformChange) {
        if (m_NeedsSorting) {
            PerformSort();
        }
    }

    // Update transform cache after sorting
    if (mode == UpdateMode::OnTransformChange && m_FramesSinceLastCheck == 0) {
        CacheNodeTransforms();
    }
}

// Property accessors
bool YSort::GetEnabled() const {
    return GetPropertyValue<bool>("enabled");
}

void YSort::SetEnabled(bool enabled) {
    SetPropertyValue<bool>("enabled", enabled);
    if (enabled) {
        m_NeedsSorting = true;
    }
}

YSort::SortAxis YSort::GetSortAxis() const {
    int axisValue = GetPropertyValue<int>("sortAxis");
    return static_cast<SortAxis>(axisValue);
}

void YSort::SetSortAxis(SortAxis axis) {
    SetPropertyValue<int>("sortAxis", static_cast<int>(axis));
    m_NeedsSorting = true;
}

bool YSort::GetInvert() const {
    return GetPropertyValue<bool>("invert");
}

void YSort::SetInvert(bool invert) {
    SetPropertyValue<bool>("invert", invert);
    m_NeedsSorting = true;
}

YSort::UpdateMode YSort::GetUpdateMode() const {
    int modeValue = GetPropertyValue<int>("updateMode");
    return static_cast<UpdateMode>(modeValue);
}

void YSort::SetUpdateMode(UpdateMode mode) {
    SetPropertyValue<int>("updateMode", static_cast<int>(mode));

    // Clear cache when switching modes
    m_TransformCache.clear();
    m_NeedsSorting = true;
    m_FramesSinceLastCheck = 0;
}

int YSort::GetZOffset() const {
    return GetPropertyValue<int>("zOffset");
}

void YSort::SetZOffset(int offset) {
    SetPropertyValue<int>("zOffset", offset);
    m_NeedsSorting = true;
}

bool YSort::GetAffectOnlySpriteChildren() const {
    return GetPropertyValue<bool>("affectOnlySpriteChildren");
}

void YSort::SetAffectOnlySpriteChildren(bool spriteOnly) {
    SetPropertyValue<bool>("affectOnlySpriteChildren", spriteOnly);
    m_NeedsSorting = true;
}

void YSort::Sort() {
    if (GetEnabled()) {
        PerformSort();
    }
}

void YSort::ForceSort() {
    m_NeedsSorting = true;
    PerformSort();
}

// Core sorting implementation
void YSort::PerformSort() {
    if (!m_Owner) {
        return;
    }

    // Gather all sortable nodes
    std::vector<NodeSortData> nodesToSort;
    nodesToSort.reserve(256); // Pre-allocate for common case

    const std::vector<std::shared_ptr<Node>>& children = m_Owner->GetChildren();

    for (const std::shared_ptr<Node>& child : children) {
        if (child) {
            GatherSortableNodes(child, nodesToSort);
        }
    }

    if (nodesToSort.empty()) {
        m_NeedsSorting = false;
        return;
    }

    // Sort based on sort value
    std::sort(nodesToSort.begin(), nodesToSort.end());

    // Apply Z positions
    ApplyZPositions(nodesToSort);

    m_NeedsSorting = false;
}

void YSort::GatherSortableNodes(const std::shared_ptr<Node>& node, std::vector<NodeSortData>& outNodes, int depth) {
    if (!node) {
        return;
    }

    bool shouldInclude = ShouldIncludeNode(node);

    if (shouldInclude) {
        NodeSortData data;
        data.node = node;
        data.sortValue = GetNodeSortValue(node);

        // Store original Z-index for tie-breaking
        std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(node);
        data.originalZIndex = node2D ? node2D->GetZIndex() : 0;

        data.originalDepth = static_cast<int>(outNodes.size());

        outNodes.push_back(data);
    }

    // Process all children recursively
    const std::vector<std::shared_ptr<Node>>& children = node->GetChildren();

    for (const std::shared_ptr<Node>& child : children) {
        if (child) {
            GatherSortableNodes(child, outNodes, depth + 1);
        }
    }
}

bool YSort::ShouldIncludeNode(const std::shared_ptr<Node>& node) const {
    if (!node) {
        return false;
    }

    // If we only affect sprite children, check for visual 2D components
    if (GetAffectOnlySpriteChildren()) {
        // Check if node has Sprite2D, AnimatedSprite2D, Image2D, Shape2D, or ColorRect component
        bool hasVisualComponent = false;

        const std::vector<std::shared_ptr<Component>>& components = node->GetComponents();
        for (const std::shared_ptr<Component>& comp : components) {
            if (!comp) {
                continue;
            }

            std::string typeName = comp->GetTypeName();
            if (typeName == "Sprite2D" || typeName == "AnimatedSprite2D" ||
                typeName == "Image2D" || typeName == "Shape2D" || typeName == "ColorRect") {
                hasVisualComponent = true;
                break;
            }
        }

        if (!hasVisualComponent) {
            return false;
        }
    }

    // Check if it's a 2D node (required for position access)
    std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(node);
    bool isNode2D = node2D != nullptr;

    return isNode2D;
}

float YSort::GetNodeSortValue(const std::shared_ptr<Node>& node) const {
    if (!node) {
        return 0.0f;
    }

    std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(node);
    if (!node2D) {
        return 0.0f;
    }

    // Get global position for sorting
    Vec2 globalPos = node2D->GetGlobalPosition();

    float sortValue = 0.0f;

    // Select axis and apply sprite bottom offset
    if (GetSortAxis() == SortAxis::Y) {
        sortValue = globalPos.y;
        // Add offset to sort by bottom of sprite instead of center
        // This way tall sprites sort based on their "feet" position
        sortValue += GetSpriteBottomOffset(node);
    }
    else { // SortAxis::X
        sortValue = globalPos.x;
        // For X sorting, add right edge offset
        sortValue += GetSpriteBottomOffset(node);
    }

    // Apply user inversion toggle
    if (GetInvert()) {
        sortValue = -sortValue;
    }

    return sortValue;
}

float YSort::GetSpriteBottomOffset(const std::shared_ptr<Node>& node) const {
    if (!node) {
        return 0.0f;
    }

    // Check for visual 2D components
    const std::vector<std::shared_ptr<Component>>& components = node->GetComponents();
    for (const std::shared_ptr<Component>& comp : components) {
        if (!comp) {
            continue;
        }

        std::string typeName = comp->GetTypeName();

        // Handle Sprite2D
        if (typeName == "Sprite2D") {
            std::shared_ptr<Sprite2D> sprite = std::dynamic_pointer_cast<Sprite2D>(comp);
            if (sprite) {
                Vec2 size = sprite->CalculateRenderSize();

                // Y-axis increases UPWARD in this coordinate system
                // For centered sprites, bottom edge is at position MINUS half height
                if (GetSortAxis() == SortAxis::Y) {
                    return -size.y * 0.5f;  // Negative to get bottom (lower Y value)
                } else {
                    return size.x * 0.5f;
                }
            }
        }
        // Handle AnimatedSprite2D
        else if (typeName == "AnimatedSprite2D") {
            std::shared_ptr<AnimatedSprite2D> animSprite = std::dynamic_pointer_cast<AnimatedSprite2D>(comp);
            if (animSprite) {
                Vec2 size = animSprite->CalculateRenderSize();

                if (GetSortAxis() == SortAxis::Y) {
                    return -size.y * 0.5f;  // Negative to get bottom (lower Y value)
                } else {
                    return size.x * 0.5f;
                }
            }
        }
        // Handle ColorRect
        else if (typeName == "ColorRect") {
            std::shared_ptr<ColorRect> colorRect = std::dynamic_pointer_cast<ColorRect>(comp);
            if (colorRect) {
                Vec2 size = colorRect->GetSize();

                if (GetSortAxis() == SortAxis::Y) {
                    return -size.y * 0.5f;  // Negative for Y-up coordinates
                } else {
                    return size.x * 0.5f;
                }
            }
        }
        // Handle Shape2D
        else if (typeName == "Shape2D") {
            std::shared_ptr<Shape2D> shape = std::dynamic_pointer_cast<Shape2D>(comp);
            if (shape) {
                // Shape2D can be circle, rectangle, etc.
                // For simplicity, use the radius or size
                float size = shape->GetSize();

                if (GetSortAxis() == SortAxis::Y) {
                    return -size * 0.5f;  // Negative for Y-up coordinates
                } else {
                    return size * 0.5f;
                }
            }
        }
        // Handle Image2D
        else if (typeName == "Image2D") {
            std::shared_ptr<Image2D> image = std::dynamic_pointer_cast<Image2D>(comp);
            if (image) {
                // Calculate size from anchors/offsets since CalculateSize() is private
                Vec2 offsetMin = image->GetOffsetMin();
                Vec2 offsetMax = image->GetOffsetMax();
                Vec2 size = offsetMax - offsetMin;

                if (GetSortAxis() == SortAxis::Y) {
                    return -size.y * 0.5f;  // Negative for Y-up coordinates
                } else {
                    return size.x * 0.5f;
                }
            }
        }
    }

    return 0.0f;
}

void YSort::ApplyZPositions(const std::vector<NodeSortData>& sortedNodes) {
    int baseOffset = GetZOffset();
    int totalCount = static_cast<int>(sortedNodes.size());

    for (size_t i = 0; i < sortedNodes.size(); ++i) {
        const std::shared_ptr<Node>& node = sortedNodes[i].node;
        if (!node) {
            continue;
        }

        std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(node);
        if (!node2D) {
            continue;
        }

        int newZ = baseOffset + (totalCount - 1 - static_cast<int>(i));

        node2D->SetZIndex(newZ);
    }
}

void YSort::CheckForTransformChanges() {
    if (!m_Owner) {
        m_NeedsSorting = true;
        return;
    }

    // If we don't have a cache yet, we need to sort and build it
    if (m_TransformCache.empty()) {
        m_NeedsSorting = true;
        return;
    }

    // Check if any cached transforms have changed
    for (const NodeTransformCache& cache : m_TransformCache) {
        std::shared_ptr<Node> nodePtr = cache.node.lock();
        if (!nodePtr) {
            // Node was deleted
            m_NeedsSorting = true;
            return;
        }

        std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(nodePtr);
        if (!node2D) {
            continue;
        }

        Vec2 currentPos = node2D->GetGlobalPosition();

        // Check if position changed (with small epsilon for floating point comparison)
        const float epsilon = 0.0001f;
        if (std::abs(currentPos.x - cache.lastPosition.x) > epsilon ||
            std::abs(currentPos.y - cache.lastPosition.y) > epsilon)
        {
            m_NeedsSorting = true;
            return;
        }
    }

    // Also check if the number of children changed
    std::vector<NodeSortData> currentNodes;
    currentNodes.reserve(m_TransformCache.size());

    const std::vector<std::shared_ptr<Node>>& children = m_Owner->GetChildren();
    for (const std::shared_ptr<Node>& child : children) {
        if (child) {
            GatherSortableNodes(child, currentNodes);
        }
    }

    if (currentNodes.size() != m_TransformCache.size()) {
        m_NeedsSorting = true;
    }
}

void YSort::CacheNodeTransforms() {
    m_TransformCache.clear();

    if (!m_Owner) {
        return;
    }

    // Gather all current sortable nodes
    std::vector<NodeSortData> currentNodes;
    currentNodes.reserve(256);

    const std::vector<std::shared_ptr<Node>>& children = m_Owner->GetChildren();
    for (const std::shared_ptr<Node>& child : children) {
        if (child) {
            GatherSortableNodes(child, currentNodes);
        }
    }

    // Cache their transforms
    m_TransformCache.reserve(currentNodes.size());

    for (const NodeSortData& nodeData : currentNodes) {
        std::shared_ptr<Node2D> node2D = std::dynamic_pointer_cast<Node2D>(nodeData.node);
        if (!node2D) {
            continue;
        }

        NodeTransformCache cache;
        cache.node = nodeData.node; // Store as weak_ptr
        cache.lastPosition = node2D->GetGlobalPosition();

        m_TransformCache.push_back(cache);
    }
}

} // namespace components
} // namespace lupine
