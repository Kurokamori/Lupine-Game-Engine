#pragma once

#include "lupine/math/Vec2.hpp"
#include <vector>

namespace lupine {
namespace navigation {

/**
 * Local collision avoidance via ORCA (Optimal Reciprocal Collision Avoidance),
 * the algorithm behind the RVO2 library. Given an agent's current state and a
 * preferred velocity, ComputeORCAVelocity returns the velocity closest to the
 * preferred one that is guaranteed collision-free for the chosen time horizon,
 * assuming neighbouring agents each take half the avoidance responsibility and
 * static disc obstacles must be avoided entirely.
 *
 * Pure math: depends only on Vec2 so it lives in the core library and is unit
 * testable in isolation.
 */

struct ORCAAgentInput {
    math::Vec2 position;
    math::Vec2 velocity;
    float radius = 1.0f;
};

struct ORCADisc {
    math::Vec2 position;
    float radius = 1.0f;
};

/**
 * @param self          The querying agent's position, current velocity, radius.
 * @param prefVelocity  The velocity the agent would take with no obstacles.
 * @param maxSpeed      Speed cap on the returned velocity.
 * @param neighbors     Other agents to avoid reciprocally (already filtered to
 *                      a neighbourhood; the querying agent must not be included).
 * @param discObstacles Static circular obstacles the agent must fully avoid.
 * @param timeStep      Simulation step (used for the collision fallback).
 * @param timeHorizon   How far ahead (seconds) to predict agent collisions.
 * @param timeHorizonObst How far ahead (seconds) to predict obstacle collisions.
 * @return A collision-free velocity, bounded by maxSpeed.
 */
math::Vec2 ComputeORCAVelocity(const ORCAAgentInput& self,
                               const math::Vec2& prefVelocity,
                               float maxSpeed,
                               const std::vector<ORCAAgentInput>& neighbors,
                               const std::vector<ORCADisc>& discObstacles,
                               float timeStep,
                               float timeHorizon,
                               float timeHorizonObst);

} // namespace navigation
} // namespace lupine
