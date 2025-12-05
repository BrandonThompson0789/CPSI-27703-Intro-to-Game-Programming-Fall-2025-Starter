#pragma once

#include "../Component.h"
#include "PathfindingBehaviorComponent.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class BodyComponent;
class PathfindingBehaviorComponent;
class SensorComponent;

/**
 * Seeks the closest sensor-satisfied object and hands its position to the
 * PathfindingBehaviorComponent.
 */
class SeekBehaviorComponent : public Component {
public:
    SeekBehaviorComponent(Object& parent);
    SeekBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~SeekBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "SeekBehaviorComponent"; }

private:
    void resolveDependencies();
    Object* findClosestTarget() const;
    void updateDestination(Object& target);
    void clearPathDestination();

    PathfindingBehaviorComponent* pathfinder;
    SensorComponent* sensor;
    BodyComponent* body;
    Object* currentTarget;

    float retargetInterval;
    float retargetTimer;
    float destinationUpdateThreshold;
    float maxSearchDistance;

    bool warnedMissingPathfinder;
    bool warnedMissingSensor;

    PathfindingBehaviorComponent::PathPoint lastIssuedDestination;
    bool hasIssuedDestination;
};

