#include "lupine/navigation/Avoidance2D.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace navigation {

using math::Vec2;

namespace {

constexpr float kRVOEpsilon = 1e-5f;

struct Line {
    Vec2 point;
    Vec2 direction;
};

float Det(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

// Solve the 1D linear program along constraint lineNo, bounded by the circle of
// the given radius and constrained by all prior lines. Returns false if the
// feasible interval is empty.
bool LinearProgram1(const std::vector<Line>& lines, size_t lineNo, float radius,
                    const Vec2& optVelocity, bool directionOpt, Vec2& result) {
    const Vec2& point = lines[lineNo].point;
    const Vec2& dir = lines[lineNo].direction;

    float dotProduct = point.Dot(dir);
    float discriminant = dotProduct * dotProduct + radius * radius - point.LengthSquared();
    if (discriminant < 0.0f) {
        return false;
    }

    float sqrtDiscriminant = std::sqrt(discriminant);
    float tLeft = -dotProduct - sqrtDiscriminant;
    float tRight = -dotProduct + sqrtDiscriminant;

    for (size_t i = 0; i < lineNo; ++i) {
        float denominator = Det(dir, lines[i].direction);
        float numerator = Det(lines[i].direction, point - lines[i].point);

        if (std::fabs(denominator) <= kRVOEpsilon) {
            if (numerator < 0.0f) {
                return false;
            }
            continue;
        }

        float t = numerator / denominator;
        if (denominator >= 0.0f) {
            tRight = std::min(tRight, t);
        } else {
            tLeft = std::max(tLeft, t);
        }
        if (tLeft > tRight) {
            return false;
        }
    }

    if (directionOpt) {
        if (optVelocity.Dot(dir) > 0.0f) {
            result = point + dir * tRight;
        } else {
            result = point + dir * tLeft;
        }
    } else {
        float t = dir.Dot(optVelocity - point);
        if (t < tLeft) {
            result = point + dir * tLeft;
        } else if (t > tRight) {
            result = point + dir * tRight;
        } else {
            result = point + dir * t;
        }
    }
    return true;
}

// Solve the 2D linear program. Returns the index of the first line that made
// the program infeasible, or lines.size() if a solution was found.
size_t LinearProgram2(const std::vector<Line>& lines, float radius,
                      const Vec2& optVelocity, bool directionOpt, Vec2& result) {
    if (directionOpt) {
        result = optVelocity * radius;
    } else if (optVelocity.LengthSquared() > radius * radius) {
        result = optVelocity.Normalized() * radius;
    } else {
        result = optVelocity;
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        if (Det(lines[i].direction, lines[i].point - result) > 0.0f) {
            Vec2 tempResult = result;
            if (!LinearProgram1(lines, i, radius, optVelocity, directionOpt, result)) {
                result = tempResult;
                return i;
            }
        }
    }
    return lines.size();
}

// Densest-feasible fallback used when LinearProgram2 reports infeasibility:
// relax the agent constraints (keeping obstacle lines hard) by minimizing the
// maximum constraint violation.
void LinearProgram3(const std::vector<Line>& lines, size_t numObstLines,
                    size_t beginLine, float radius, Vec2& result) {
    float distance = 0.0f;

    for (size_t i = beginLine; i < lines.size(); ++i) {
        if (Det(lines[i].direction, lines[i].point - result) > distance) {
            std::vector<Line> projLines(lines.begin(), lines.begin() + static_cast<long>(numObstLines));

            for (size_t j = numObstLines; j < i; ++j) {
                Line line;
                float determinant = Det(lines[i].direction, lines[j].direction);

                if (std::fabs(determinant) <= kRVOEpsilon) {
                    if (lines[i].direction.Dot(lines[j].direction) > 0.0f) {
                        continue;
                    }
                    line.point = (lines[i].point + lines[j].point) * 0.5f;
                } else {
                    float factor = Det(lines[j].direction, lines[i].point - lines[j].point) / determinant;
                    line.point = lines[i].point + lines[i].direction * factor;
                }

                line.direction = (lines[j].direction - lines[i].direction).Normalized();
                projLines.push_back(line);
            }

            Vec2 tempResult = result;
            Vec2 optDir(-lines[i].direction.y, lines[i].direction.x);
            if (LinearProgram2(projLines, radius, optDir, true, result) < projLines.size()) {
                result = tempResult;
            }
            distance = Det(lines[i].direction, lines[i].point - result);
        }
    }
}

// Build the ORCA half-plane constraint for one neighbour or obstacle disc.
Line BuildORCALine(const Vec2& relativePosition, const Vec2& relativeVelocity,
                   float combinedRadius, float invTimeHorizon, float timeStep,
                   float responsibility, const Vec2& selfVelocity) {
    Line line;
    Vec2 u;

    float distSq = relativePosition.LengthSquared();
    float combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq > combinedRadiusSq) {
        Vec2 w = relativeVelocity - relativePosition * invTimeHorizon;
        float wLengthSq = w.LengthSquared();
        float dotProduct1 = w.Dot(relativePosition);

        if (dotProduct1 < 0.0f && dotProduct1 * dotProduct1 > combinedRadiusSq * wLengthSq) {
            // Project on the cut-off circle.
            float wLength = std::sqrt(wLengthSq);
            Vec2 unitW = (wLength > kRVOEpsilon) ? (w / wLength) : Vec2::Zero();
            line.direction = Vec2(unitW.y, -unitW.x);
            u = unitW * (combinedRadius * invTimeHorizon - wLength);
        } else {
            // Project on one of the legs.
            float leg = std::sqrt(std::max(0.0f, distSq - combinedRadiusSq));
            if (Det(relativePosition, w) > 0.0f) {
                line.direction = Vec2(relativePosition.x * leg - relativePosition.y * combinedRadius,
                                      relativePosition.x * combinedRadius + relativePosition.y * leg) / distSq;
            } else {
                line.direction = Vec2(relativePosition.x * leg + relativePosition.y * combinedRadius,
                                      -relativePosition.x * combinedRadius + relativePosition.y * leg) / distSq * -1.0f;
            }
            float dotProduct2 = relativeVelocity.Dot(line.direction);
            u = line.direction * dotProduct2 - relativeVelocity;
        }
    } else {
        // Already colliding; resolve over a single time step.
        float invTimeStep = (timeStep > kRVOEpsilon) ? (1.0f / timeStep) : 0.0f;
        Vec2 w = relativeVelocity - relativePosition * invTimeStep;
        float wLength = w.Length();
        Vec2 unitW = (wLength > kRVOEpsilon) ? (w / wLength) : Vec2::Zero();
        line.direction = Vec2(unitW.y, -unitW.x);
        u = unitW * (combinedRadius * invTimeStep - wLength);
    }

    line.point = selfVelocity + u * responsibility;
    return line;
}

} // namespace

Vec2 ComputeORCAVelocity(const ORCAAgentInput& self,
                         const Vec2& prefVelocity,
                         float maxSpeed,
                         const std::vector<ORCAAgentInput>& neighbors,
                         const std::vector<ORCADisc>& discObstacles,
                         float timeStep,
                         float timeHorizon,
                         float timeHorizonObst) {
    std::vector<Line> lines;

    float invTimeHorizonObst = (timeHorizonObst > kRVOEpsilon) ? (1.0f / timeHorizonObst) : 0.0f;
    // Obstacle lines first: agent takes full responsibility for avoiding them.
    for (const ORCADisc& obstacle : discObstacles) {
        Vec2 relativePosition = obstacle.position - self.position;
        Vec2 relativeVelocity = self.velocity; // obstacle velocity is zero
        float combinedRadius = self.radius + obstacle.radius;
        lines.push_back(BuildORCALine(relativePosition, relativeVelocity, combinedRadius,
                                      invTimeHorizonObst, timeStep, 1.0f, self.velocity));
    }
    size_t numObstLines = lines.size();

    float invTimeHorizon = (timeHorizon > kRVOEpsilon) ? (1.0f / timeHorizon) : 0.0f;
    for (const ORCAAgentInput& other : neighbors) {
        Vec2 relativePosition = other.position - self.position;
        Vec2 relativeVelocity = self.velocity - other.velocity;
        float combinedRadius = self.radius + other.radius;
        lines.push_back(BuildORCALine(relativePosition, relativeVelocity, combinedRadius,
                                      invTimeHorizon, timeStep, 0.5f, self.velocity));
    }

    Vec2 result;
    size_t lineFail = LinearProgram2(lines, maxSpeed, prefVelocity, false, result);
    if (lineFail < lines.size()) {
        LinearProgram3(lines, numObstLines, lineFail, maxSpeed, result);
    }
    return result;
}

} // namespace navigation
} // namespace lupine
