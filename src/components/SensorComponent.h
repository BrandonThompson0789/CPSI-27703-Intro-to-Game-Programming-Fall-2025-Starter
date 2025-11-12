#pragma once

#include "Component.h"
#include "SensorTypes.h"

#include <box2d/box2d.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

class BodyComponent;
class InteractComponent;

class SensorComponent : public Component {
public:
    SensorComponent(Object& parent);
    SensorComponent(Object& parent, const nlohmann::json& data);

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "SensorComponent"; }

    void setTargetNames(const std::vector<std::string>& names);
    const std::vector<std::string>& getTargetNames() const { return targetNames; }

    // Debug information methods
    bool requiresCollision() const { return requireCollision; }
    bool requiresInteractInput() const { return requireInteractInput; }
    bool hasDistanceCondition() const { return maxDistance > 0.0f; }
    float getMaxDistance() const { return maxDistance; }
    int getConditionCount() const;
    int getSatisfiedConditionCount(const std::vector<Object*>& allObjects) const;
    std::vector<Object*> getTargetObjects(const std::vector<std::unique_ptr<Object>>& allObjects) const;

private:
    bool requireCollision;
    bool requireInteractInput;
    float maxDistance;
    float holdDuration;
    SenseTypeMask requiredSenses;

    std::vector<std::string> targetNames;
    std::vector<Object*> targetCache;
    bool targetCacheDirty;

    BodyComponent* cachedBody;
    b2BodyId cachedBodyId;
    bool usingSensorFixtures;
    std::vector<b2ShapeId> shapeCache;

    std::unordered_map<Object*, float> conditionTimers;
    bool locked;

    void initializeDefaults();
    void refreshBodyCache();
    void refreshShapeCache();
    void refreshTargetCache();
    void rebuildTargetCache();
    void updateSenseMask();

    std::vector<Object*> gatherCandidates() const;
    bool isInstigatorEligible(Object& instigator) const;
    bool verifyCollisionCondition(Object& instigator) const;
    bool verifyDistanceCondition(Object& instigator) const;
    bool verifyInteractCondition(InteractComponent& interact) const;

    void advanceTimersForCandidates(const std::vector<Object*>& candidates, float deltaTime);
    void trigger(Object& instigator);
    void cleanExpiredTimers(const std::unordered_set<Object*>& processed);

    static float computeDistanceSquared(Object& a, Object& b);
};


