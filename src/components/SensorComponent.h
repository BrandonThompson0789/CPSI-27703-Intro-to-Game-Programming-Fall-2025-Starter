#pragma once

#include "Component.h"
#include "SensorTypes.h"

#include <box2d/box2d.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>

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
    int getSatisfyingObjectCount(const std::vector<Object*>& allObjects) const;  // Count objects currently satisfying all conditions
    std::vector<Object*> getTargetObjects(const std::vector<std::unique_ptr<Object>>& allObjects) const;
    std::vector<Object*> getSatisfiedObjects() const;
    std::vector<Object*> getSatisfiedInstigators() const;
    bool isInstigatorSatisfied(const Object* object) const;
    
    // Getters for new sensor types (for debug drawing)
    bool requiresGlobalValue() const { return requireGlobalValue; }
    const std::string& getGlobalValueName() const { return globalValueName; }
    const std::string& getGlobalValueComparison() const { return globalValueComparison; }
    float getGlobalValueThreshold() const { return globalValueThreshold; }
    bool requiresBoxZone() const { return requireBoxZone; }
    void getBoxZoneBounds(float& minX, float& minY, float& maxX, float& maxY) const {
        minX = boxZoneMinX; minY = boxZoneMinY; maxX = boxZoneMaxX; maxY = boxZoneMaxY;
    }
    bool boxZoneRequiresFull() const { return boxZoneRequireFull; }

private:
    bool requireCollision;
    bool requireInteractInput;
    float maxDistance;
    float holdDuration;
    SenseTypeMask requiredSenses;
    
    // New sensor type configurations
    bool requireInputActivity;
    std::string inputActivityType;  // "controller_connect", "client_join", etc.
    bool requireBoxZone;
    float boxZoneMinX, boxZoneMinY, boxZoneMaxX, boxZoneMaxY;
    bool boxZoneRequireFull;  // true = fully inside, false = partially inside
    bool requireGlobalValue;
    std::string globalValueName;
    std::string globalValueComparison;  // ">", ">=", "==", "<=", "<", "!="
    float globalValueThreshold;

    std::vector<std::string> targetNames;
    std::vector<std::regex> targetRegexes;  // Compiled regex patterns
    bool useRegex;  // Whether to use regex matching
    bool allowSelfTrigger;  // Whether sensor can trigger itself
    
    // Allowed instigator names (objects that can trigger without InteractComponent)
    std::vector<std::string> allowedInstigatorNames;
    std::vector<std::regex> allowedInstigatorRegexes;  // Compiled regex patterns
    bool useRegexForInstigators;  // Whether to use regex matching for instigators
    bool includeInteractComponent;  // If true, include objects with InteractComponent in addition to allowedInstigators (whitelist mode)
    
    // Targets to trigger when conditions are no longer satisfied
    std::vector<std::string> unsatisfiedTargetNames;
    std::vector<std::regex> unsatisfiedTargetRegexes;  // Compiled regex patterns
    std::vector<Object*> unsatisfiedTargetCache;
    bool unsatisfiedTargetCacheDirty;
    
    std::vector<Object*> targetCache;
    bool targetCacheDirty;

    BodyComponent* cachedBody;
    b2BodyId cachedBodyId;
    bool usingSensorFixtures;
    std::vector<b2ShapeId> shapeCache;

    std::unordered_map<Object*, float> conditionTimers;
    std::unordered_set<Object*> triggeredInstigators;  // Track which instigators have triggered (per-instigator satisfaction)
    std::unordered_set<Object*> previouslySatisfiedInstigators;  // Track which instigators were previously satisfied (for unsatisfied triggers)

    void initializeDefaults();
    void refreshBodyCache();
    void refreshShapeCache();
    void refreshTargetCache();
    void rebuildTargetCache();
    void refreshUnsatisfiedTargetCache();
    void rebuildUnsatisfiedTargetCache();
    void updateSenseMask();

    std::vector<Object*> gatherCandidates() const;
    bool isInstigatorEligible(Object& instigator) const;
    bool verifyCollisionCondition(Object& instigator) const;
    bool verifyDistanceCondition(Object& instigator) const;
    bool verifyInteractCondition(InteractComponent& interact) const;
    bool verifyInputActivityCondition() const;
    bool verifyBoxZoneCondition(Object& instigator) const;
    bool verifyGlobalValueCondition() const;

    void advanceTimersForCandidates(const std::vector<Object*>& candidates, float deltaTime);
    void trigger(Object& instigator);
    void triggerUnsatisfied(Object& instigator);
    void cleanExpiredTimers(const std::unordered_set<Object*>& processed);

    static float computeDistanceSquared(Object& a, Object& b);
};


