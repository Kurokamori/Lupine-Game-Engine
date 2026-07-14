#pragma once

#include "lupine/core/Node.hpp"
#include "lupine/math/Mat4.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec4.hpp"

namespace lupine {
namespace physics2d {

/**
 * Box2D bodies live in world space, and so does everything that reads them back: the 2D queries,
 * the RayCast2D/ShapeCast2D components, the collision debug shapes and the character sweeps. A
 * Node2D transform, by contrast, is relative to its parent. Physics-body components therefore
 * write the node's GLOBAL transform into the body and convert the body transform back to a LOCAL
 * one when reading it out - the same contract the 3D body components follow.
 */

/**
 * Exact inverse of Node2D::GetGlobalPosition(), which transforms the local position by the
 * parent's global matrix. Inverting that same matrix keeps the round trip lossless even when an
 * ancestor carries a non-uniform scale or shear, which no (rotation, scale) pair could represent.
 */
inline math::Vec2 GlobalToLocalPosition(const core::Node2D& node, const math::Vec2& globalPosition) {
    core::Node2D* parent2D = dynamic_cast<core::Node2D*>(node.GetParent());
    if (!parent2D) {
        return globalPosition;
    }

    const math::Mat4 inverseParent = parent2D->GetGlobalTransformMatrix().Inverse();
    const math::Vec4 local = inverseParent * math::Vec4(globalPosition.x, globalPosition.y, 0.0f, 1.0f);
    return math::Vec2(local.x, local.y);
}

/**
 * Inverse of Node2D::GetGlobalRotation(), which simply sums the rotations up the chain.
 */
inline float GlobalToLocalRotation(const core::Node2D& node, float globalRotation) {
    core::Node2D* parent2D = dynamic_cast<core::Node2D*>(node.GetParent());
    if (!parent2D) {
        return globalRotation;
    }

    return globalRotation - parent2D->GetGlobalRotation();
}

} // namespace physics2d
} // namespace lupine
