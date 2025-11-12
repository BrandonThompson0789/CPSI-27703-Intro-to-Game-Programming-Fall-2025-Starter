#include "SensorComponent.h"

#include "BodyComponent.h"
#include "ComponentLibrary.h"
#include "InteractComponent.h"
#include "../Engine.h"
#include "../Object.h"
#include "../SensorEventManager.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <tuple>

namespace {

float clampNonNegative(float value) {
    return std::max(0.0f, value);
}

} // namespace

SensorComponent::SensorComponent(Object& parent)
    : Component(parent) {
    initializeDefaults();
}

SensorComponent::SensorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    initializeDefaults();

    requireCollision = data.value("requireCollision", requireCollision);
    requireInteractInput = data.value("requireInteractInput", requireInteractInput);
    maxDistance = data.value("maxDistance", maxDistance);
    holdDuration = clampNonNegative(data.value("holdDuration", holdDuration));

    if (data.contains("targetObjects")) {
        if (data["targetObjects"].is_array()) {
            targetNames = data["targetObjects"].get<std::vector<std::string>>();
        } else if (data["targetObjects"].is_string()) {
            targetNames = {data["targetObjects"].get<std::string>()};
        }
        targetCacheDirty = true;
    }

    updateSenseMask();
}

void SensorComponent::initializeDefaults() {
    requireCollision = false;
    requireInteractInput = false;
    maxDistance = 0.0f;
    holdDuration = 0.0f;
    requiredSenses = makeSenseMask(SenseType::None);
    targetCacheDirty = true;
    cachedBody = nullptr;
    cachedBodyId = b2_nullBodyId;
    usingSensorFixtures = false;
    locked = false;
    shapeCache.clear();
    conditionTimers.clear();
}

void SensorComponent::updateSenseMask() {
    SenseTypeMask mask = makeSenseMask(SenseType::None);
    if (requireCollision) {
        mask = addSenseToMask(mask, SenseType::Collision);
    }
    if (maxDistance > 0.0f) {
        mask = addSenseToMask(mask, SenseType::Distance);
    }
    if (requireInteractInput) {
        mask = addSenseToMask(mask, SenseType::InteractInput);
    }
    requiredSenses = mask;
}

void SensorComponent::refreshBodyCache() {
    auto* body = parent().getComponent<BodyComponent>();
    if (body != cachedBody) {
        cachedBody = body;
    }
    cachedBodyId = cachedBody ? cachedBody->getBodyId() : b2_nullBodyId;
}

void SensorComponent::refreshShapeCache() {
    refreshBodyCache();
    shapeCache.clear();
    usingSensorFixtures = false;

    if (!cachedBody || B2_IS_NULL(cachedBodyId)) {
        return;
    }

    const int shapeCount = b2Body_GetShapeCount(cachedBodyId);
    if (shapeCount <= 0) {
        return;
    }

    shapeCache.resize(shapeCount);
    b2Body_GetShapes(cachedBodyId, shapeCache.data(), shapeCount);
    for (const b2ShapeId shapeId : shapeCache) {
        if (b2Shape_IsSensor(shapeId)) {
            usingSensorFixtures = true;
        }
    }
}

void SensorComponent::rebuildTargetCache() {
    targetCache.clear();
    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    for (const auto& objectPtr : engine->getObjects()) {
        if (!objectPtr) {
            continue;
        }
        Object* obj = objectPtr.get();
        if (!obj || !Object::isAlive(obj)) {
            continue;
        }
        if (obj == &parent()) {
            continue;
        }
        if (targetNames.empty()) {
            continue;
        }
        if (std::find(targetNames.begin(), targetNames.end(), obj->getName()) != targetNames.end()) {
            targetCache.push_back(obj);
        }
    }
    targetCacheDirty = false;
}

void SensorComponent::refreshTargetCache() {
    if (!targetCacheDirty) {
        for (Object* target : targetCache) {
            if (!target || !Object::isAlive(target)) {
                targetCacheDirty = true;
                break;
            }
        }
    }

    if (targetCacheDirty) {
        rebuildTargetCache();
    }
}

void SensorComponent::setTargetNames(const std::vector<std::string>& names) {
    targetNames = names;
    targetCacheDirty = true;
}

std::vector<Object*> SensorComponent::gatherCandidates() const {
    std::unordered_set<Object*> uniqueCandidates;

    SensorEventManager& eventManager = SensorEventManager::getInstance();
    if (!shapeCache.empty()) {
        for (const b2ShapeId shapeId : shapeCache) {
            const auto contacting = eventManager.getContactingObjects(shapeId);
            uniqueCandidates.insert(contacting.begin(), contacting.end());
            if (usingSensorFixtures) {
                const auto overlapping = eventManager.getSensorOverlappingObjects(shapeId);
                uniqueCandidates.insert(overlapping.begin(), overlapping.end());
            }
        }
    }

    //if (!requireCollision) {
        Engine* engine = Object::getEngine();
        if (engine) {
            for (const auto& objectPtr : engine->getObjects()) {
                Object* obj = objectPtr.get();
                if (!obj || obj == &parent()) {
                    continue;
                }
                if (!Object::isAlive(obj)) {
                    continue;
                }
                if (!obj->getComponent<InteractComponent>()) {
                    continue;
                }
                uniqueCandidates.insert(obj);
            }
        }
    //}

    std::vector<Object*> candidates;
    candidates.reserve(uniqueCandidates.size());
    for (Object* obj : uniqueCandidates) {
        if (obj && obj != &parent() && Object::isAlive(obj)) {
            candidates.push_back(obj);
        }
    }
    return candidates;
}

bool SensorComponent::isInstigatorEligible(Object& instigator) const {
    auto* interact = instigator.getComponent<InteractComponent>();
    if (!interact) {
        return false;
    }
    if (senseMaskHas(requiredSenses, SenseType::Collision) && !interact->supportsSense(SenseType::Collision)) {
        return false;
    }
    if (senseMaskHas(requiredSenses, SenseType::Distance) && !interact->supportsSense(SenseType::Distance)) {
        return false;
    }
    if (senseMaskHas(requiredSenses, SenseType::InteractInput) && !interact->supportsSense(SenseType::InteractInput)) {
        return false;
    }
    return true;
}

bool SensorComponent::verifyCollisionCondition(Object& instigator) const {
    if (!requireCollision) {
        return true;
    }
    SensorEventManager& eventManager = SensorEventManager::getInstance();
    for (const b2ShapeId shapeId : shapeCache) {
        if (eventManager.hasContactWith(shapeId, &instigator)) {
            return true;
        }
        if (usingSensorFixtures && eventManager.hasSensorOverlapWith(shapeId, &instigator)) {
            return true;
        }
    }
    return false;
}

float SensorComponent::computeDistanceSquared(Object& a, Object& b) {
    auto* bodyA = a.getComponent<BodyComponent>();
    auto* bodyB = b.getComponent<BodyComponent>();
    if (!bodyA || !bodyB) {
        return std::numeric_limits<float>::infinity();
    }

    float ax = 0.0f, ay = 0.0f, angleA = 0.0f;
    float bx = 0.0f, by = 0.0f, angleB = 0.0f;
    std::tie(ax, ay, angleA) = bodyA->getPosition();
    std::tie(bx, by, angleB) = bodyB->getPosition();
    (void)angleA;
    (void)angleB;

    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

bool SensorComponent::verifyDistanceCondition(Object& instigator) const {
    if (maxDistance <= 0.0f) {
        return true;
    }
    if (!cachedBody || B2_IS_NULL(cachedBodyId)) {
        return false;
    }
    const float distanceSq = computeDistanceSquared(parent(), instigator);
    return distanceSq <= (maxDistance * maxDistance);
}

bool SensorComponent::verifyInteractCondition(InteractComponent& interact) const {
    if (!requireInteractInput) {
        return true;
    }
    return interact.isInteractPressed();
}

void SensorComponent::cleanExpiredTimers(const std::unordered_set<Object*>& processed) {
    for (auto it = conditionTimers.begin(); it != conditionTimers.end();) {
        Object* instigator = it->first;
        if (!instigator || !Object::isAlive(instigator) || processed.find(instigator) == processed.end()) {
            it = conditionTimers.erase(it);
        } else {
            ++it;
        }
    }
}

void SensorComponent::advanceTimersForCandidates(const std::vector<Object*>& candidates, float deltaTime) {
    std::unordered_set<Object*> processed;
    bool anySatisfied = false;

    for (Object* candidate : candidates) {
        if (!candidate || candidate == &parent() || !Object::isAlive(candidate)) {
            continue;
        }

        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact) {
            conditionTimers.erase(candidate);
            continue;
        }

        if (!isInstigatorEligible(*candidate)) {
            conditionTimers.erase(candidate);
            continue;
        }

        const bool collisionOk = verifyCollisionCondition(*candidate);
        const bool distanceOk = verifyDistanceCondition(*candidate);
        const bool interactOk = verifyInteractCondition(*interact);

        processed.insert(candidate);

        const bool conditionsMet = collisionOk && distanceOk && interactOk;
        if (conditionsMet) {
            anySatisfied = true;
            float nextTimer = 0.0f;
            auto existing = conditionTimers.find(candidate);
            if (existing != conditionTimers.end()) {
                nextTimer = existing->second;
            }
            nextTimer += deltaTime;
            conditionTimers[candidate] = nextTimer;

            const float requiredHold = clampNonNegative(holdDuration);
            if (!locked && nextTimer >= requiredHold) {
                trigger(*candidate);
                locked = true;
            }
        } else {
            conditionTimers[candidate] = 0.0f;
        }
    }

    cleanExpiredTimers(processed);

    if (locked && !anySatisfied) {
        locked = false;
    }
}

void SensorComponent::trigger(Object& instigator) {
    refreshTargetCache();

    const std::string sensorName = parent().getName().empty() ? "<unnamed_sensor>" : parent().getName();
    const std::string instigatorName = instigator.getName().empty() ? "<unnamed_instigator>" : instigator.getName();
    std::cout << "[SensorComponent] Sensor '" << sensorName << "' triggered by '" << instigatorName << "'. Target count: "
              << targetCache.size() << std::endl;

    if (targetCache.empty()) {
        return;
    }

    for (Object* target : targetCache) {
        if (!target || !Object::isAlive(target)) {
            targetCacheDirty = true;
            continue;
        }
        target->use(instigator);
    }
}

void SensorComponent::update(float deltaTime) {
    updateSenseMask();
    refreshShapeCache();
    refreshTargetCache();

    if (requireCollision && shapeCache.empty()) {
        // Without shapes we can't evaluate collision; ensure timers are cleared.
        conditionTimers.clear();
        locked = false;
        return;
    }

    const auto candidates = gatherCandidates();
    advanceTimersForCandidates(candidates, deltaTime);
}

void SensorComponent::draw() {
    // No direct rendering; sensor is logic-only.
}

nlohmann::json SensorComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["requireCollision"] = requireCollision;
    data["requireInteractInput"] = requireInteractInput;
    if (maxDistance > 0.0f) {
        data["maxDistance"] = maxDistance;
    }
    if (holdDuration > 0.0f) {
        data["holdDuration"] = holdDuration;
    }
    if (!targetNames.empty()) {
        data["targetObjects"] = targetNames;
    }
    return data;
}

int SensorComponent::getConditionCount() const {
    int count = 0;
    if (requireCollision) {
        ++count;
    }
    if (maxDistance > 0.0f) {
        ++count;
    }
    if (requireInteractInput) {
        ++count;
    }
    return count;
}

int SensorComponent::getSatisfiedConditionCount(const std::vector<Object*>& allObjects) const {
    int satisfied = 0;
    
    // Get candidates that might satisfy conditions
    // Note: This uses cached data which should be up-to-date if update() was called
    std::vector<Object*> candidates = gatherCandidates();
    
    // Initialize satisfaction flags - only check conditions that are actually required
    bool collisionSatisfied = !requireCollision; // If not required, doesn't need to be satisfied
    bool distanceSatisfied = maxDistance <= 0.0f; // If no distance requirement, doesn't need to be satisfied
    bool interactSatisfied = !requireInteractInput; // If not required, doesn't need to be satisfied
    
    // Check each candidate to see if it satisfies conditions
    for (Object* candidate : candidates) {
        if (!candidate || candidate == &parent() || !Object::isAlive(candidate)) {
            continue;
        }
        
        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact || !isInstigatorEligible(*candidate)) {
            continue;
        }
        
        // Check collision condition if required and not yet satisfied
        if (requireCollision && !collisionSatisfied) {
            if (verifyCollisionCondition(*candidate)) {
                collisionSatisfied = true;
            }
        }
        
        // Check distance condition if required and not yet satisfied
        if (maxDistance > 0.0f && !distanceSatisfied) {
            if (verifyDistanceCondition(*candidate)) {
                distanceSatisfied = true;
            }
        }
        
        // Check interact condition if required and not yet satisfied
        if (requireInteractInput && !interactSatisfied) {
            if (verifyInteractCondition(*interact)) {
                interactSatisfied = true;
            }
        }
        
        // Early exit if all required conditions are satisfied
        if (collisionSatisfied && distanceSatisfied && interactSatisfied) {
            break;
        }
    }
    
    // Count how many of the active conditions are satisfied
    if (requireCollision && collisionSatisfied) {
        ++satisfied;
    }
    if (maxDistance > 0.0f && distanceSatisfied) {
        ++satisfied;
    }
    if (requireInteractInput && interactSatisfied) {
        ++satisfied;
    }
    
    return satisfied;
}

std::vector<Object*> SensorComponent::getTargetObjects(const std::vector<std::unique_ptr<Object>>& allObjects) const {
    std::vector<Object*> targets;
    if (targetNames.empty()) {
        return targets;
    }
    
    for (const auto& objectPtr : allObjects) {
        if (!objectPtr) {
            continue;
        }
        Object* obj = objectPtr.get();
        if (!obj || !Object::isAlive(obj)) {
            continue;
        }
        if (obj == &parent()) {
            continue;
        }
        const std::string& objName = obj->getName();
        if (std::find(targetNames.begin(), targetNames.end(), objName) != targetNames.end()) {
            targets.push_back(obj);
        }
    }
    
    return targets;
}

static ComponentRegistrar<SensorComponent> registrar("SensorComponent");

