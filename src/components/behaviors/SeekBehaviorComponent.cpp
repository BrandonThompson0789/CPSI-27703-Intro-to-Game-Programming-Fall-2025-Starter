#include "SeekBehaviorComponent.h"

#include "PathfindingBehaviorComponent.h"
#include "../BodyComponent.h"
#include "../SensorComponent.h"
#include "../ComponentLibrary.h"
#include "../../Engine.h"
#include "../../Object.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
float squaredDistance(float ax, float ay, float bx, float by) {
    float dx = ax - bx;
    float dy = ay - by;
    return dx * dx + dy * dy;
}
} // namespace

SeekBehaviorComponent::SeekBehaviorComponent(Object& parent)
    : Component(parent)
    , pathfinder(nullptr)
    , sensor(nullptr)
    , body(nullptr)
    , currentTarget(nullptr)
    , retargetInterval(0.5f)
    , retargetTimer(0.0f)
    , destinationUpdateThreshold(24.0f)
    , maxSearchDistance(0.0f)
    , warnedMissingPathfinder(false)
    , warnedMissingSensor(false)
    , hasIssuedDestination(false) {
    lastIssuedDestination = {0.0f, 0.0f};
    resolveDependencies();
}

SeekBehaviorComponent::SeekBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , pathfinder(nullptr)
    , sensor(nullptr)
    , body(nullptr)
    , currentTarget(nullptr)
    , retargetInterval(data.value("retargetInterval", 0.5f))
    , retargetTimer(0.0f)
    , destinationUpdateThreshold(data.value("destinationUpdateThreshold", 24.0f))
    , maxSearchDistance(data.value("maxSearchDistance", 0.0f))
    , warnedMissingPathfinder(false)
    , warnedMissingSensor(false)
    , hasIssuedDestination(false) {
    lastIssuedDestination = {0.0f, 0.0f};

    resolveDependencies();
}

void SeekBehaviorComponent::resolveDependencies() {
    pathfinder = parent().getComponent<PathfindingBehaviorComponent>();
    sensor = parent().getComponent<SensorComponent>();
    body = parent().getComponent<BodyComponent>();
    if (!pathfinder && !warnedMissingPathfinder) {
        std::cerr << "[SeekBehaviorComponent] Warning: '" << parent().getName() << "' missing PathfindingBehaviorComponent.\n";
        warnedMissingPathfinder = true;
    }
    if (!sensor && !warnedMissingSensor) {
        std::cerr << "[SeekBehaviorComponent] Warning: '" << parent().getName() << "' missing SensorComponent.\n";
        warnedMissingSensor = true;
    }
    if (!body) {
        std::cerr << "[SeekBehaviorComponent] Warning: '" << parent().getName() << "' missing BodyComponent.\n";
    }
}

Object* SeekBehaviorComponent::findClosestTarget() const {
    if (!body || !sensor) {
        return nullptr;
    }

    auto [selfX, selfY, _angle] = body->getPosition();
    float maxDistanceSq = maxSearchDistance > 0.0f ? maxSearchDistance * maxSearchDistance : std::numeric_limits<float>::infinity();

    Object* closest = nullptr;
    float closestDistSq = std::numeric_limits<float>::infinity();

    auto considerCandidate = [&](Object* candidate) {
        if (!candidate || candidate == &parent() || !Object::isAlive(candidate)) {
            return;
        }
        BodyComponent* candidateBody = candidate->getComponent<BodyComponent>();
        if (!candidateBody) {
            return;
        }
        auto [targetX, targetY, _] = candidateBody->getPosition();
        float distSq = squaredDistance(selfX, selfY, targetX, targetY);
        if (distSq > maxDistanceSq) {
            return;
        }
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closest = candidate;
        }
    };

    auto instigators = sensor->getSatisfiedObjects();
    for (Object* instigator : instigators) {
        considerCandidate(instigator);
    }

    return closest;
}

void SeekBehaviorComponent::updateDestination(Object& target) {
    if (!pathfinder) {
        return;
    }
    if (!Object::isAlive(&target)) {
        currentTarget = nullptr;
        clearPathDestination();
        return;
    }

    BodyComponent* targetBody = target.getComponent<BodyComponent>();
    if (!targetBody) {
        return;
    }

    auto [targetX, targetY, _] = targetBody->getPosition();
    PathfindingBehaviorComponent::PathPoint newGoal{targetX, targetY};

    if (!hasIssuedDestination) {
        pathfinder->setDestination(newGoal);
        lastIssuedDestination = newGoal;
        hasIssuedDestination = true;
        return;
    }

    float deltaSq = squaredDistance(lastIssuedDestination.x, lastIssuedDestination.y, newGoal.x, newGoal.y);
    if (deltaSq >= destinationUpdateThreshold * destinationUpdateThreshold) {
        pathfinder->setDestination(newGoal);
        lastIssuedDestination = newGoal;
    }
}

void SeekBehaviorComponent::clearPathDestination() {
    if (pathfinder && pathfinder->hasDestination()) {
        pathfinder->clearDestination();
    }
    hasIssuedDestination = false;
}

void SeekBehaviorComponent::update(float deltaTime) {
    if (!body || !pathfinder || !sensor) {
        resolveDependencies();
    }
    if (!sensor) {
        sensor = parent().getComponent<SensorComponent>();
    }
    if (!body || !pathfinder || !sensor) {
        return;
    }

    retargetTimer -= deltaTime;

    auto invalidateTarget = [&]() {
        currentTarget = nullptr;
        hasIssuedDestination = false;
        clearPathDestination();
    };

    if (currentTarget) {
        bool valid = Object::isAlive(currentTarget) && sensor && sensor->isInstigatorSatisfied(currentTarget);
        if (valid && maxSearchDistance > 0.0f) {
            auto [selfX, selfY, _] = body->getPosition();
            if (BodyComponent* targetBody = currentTarget->getComponent<BodyComponent>()) {
                auto [targetX, targetY, _a] = targetBody->getPosition();
                float distSq = squaredDistance(selfX, selfY, targetX, targetY);
                if (distSq > maxSearchDistance * maxSearchDistance) {
                    valid = false;
                }
            } else {
                valid = false;
            }
        }
        if (!valid) {
            invalidateTarget();
        }
    }

    if (!currentTarget && retargetTimer <= 0.0f) {
        currentTarget = findClosestTarget();
        retargetTimer = retargetInterval;
        hasIssuedDestination = false;
        if (!currentTarget) {
            clearPathDestination();
        }
    }

    if (currentTarget) {
        updateDestination(*currentTarget);
    }
}

nlohmann::json SeekBehaviorComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["retargetInterval"] = retargetInterval;
    data["destinationUpdateThreshold"] = destinationUpdateThreshold;
    data["maxSearchDistance"] = maxSearchDistance;
    return data;
}

static ComponentRegistrar<SeekBehaviorComponent> registrar("SeekBehaviorComponent");

