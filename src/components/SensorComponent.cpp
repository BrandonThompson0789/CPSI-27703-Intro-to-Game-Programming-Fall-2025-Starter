#include "SensorComponent.h"

#include "BodyComponent.h"
#include "ComponentLibrary.h"
#include "InteractComponent.h"
#include "../Engine.h"
#include "../Object.h"
#include "../SensorEventManager.h"
#include "../GlobalValueManager.h"
#include "../InputManager.h"
#include "../HostManager.h"
#include "../ClientManager.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <tuple>
#include <regex>
#include <unordered_map>

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
    useRegex = data.value("useRegex", useRegex);
    allowSelfTrigger = data.value("allowSelfTrigger", allowSelfTrigger);
    useRegexForInstigators = data.value("useRegexForInstigators", useRegexForInstigators);
    includeInteractComponent = data.value("includeInteractComponent", includeInteractComponent);

    if (data.contains("targetObjects")) {
        if (data["targetObjects"].is_array()) {
            targetNames = data["targetObjects"].get<std::vector<std::string>>();
        } else if (data["targetObjects"].is_string()) {
            targetNames = {data["targetObjects"].get<std::string>()};
        }
        targetCacheDirty = true;
    }
    
    if (data.contains("unsatisfiedTargetObjects")) {
        if (data["unsatisfiedTargetObjects"].is_array()) {
            unsatisfiedTargetNames = data["unsatisfiedTargetObjects"].get<std::vector<std::string>>();
        } else if (data["unsatisfiedTargetObjects"].is_string()) {
            unsatisfiedTargetNames = {data["unsatisfiedTargetObjects"].get<std::string>()};
        }
        unsatisfiedTargetCacheDirty = true;
    }
    
    if (data.contains("allowedInstigators")) {
        if (data["allowedInstigators"].is_array()) {
            allowedInstigatorNames = data["allowedInstigators"].get<std::vector<std::string>>();
        } else if (data["allowedInstigators"].is_string()) {
            allowedInstigatorNames = {data["allowedInstigators"].get<std::string>()};
        }
    }

    // New sensor type configurations
    if (data.contains("requireInputActivity")) {
        requireInputActivity = data["requireInputActivity"].get<bool>();
        if (data.contains("inputActivityType")) {
            inputActivityType = data["inputActivityType"].get<std::string>();
        }
    }
    
    if (data.contains("requireBoxZone")) {
        requireBoxZone = data["requireBoxZone"].get<bool>();
        if (data.contains("boxZone")) {
            const auto& boxZone = data["boxZone"];
            boxZoneMinX = boxZone.value("minX", 0.0f);
            boxZoneMinY = boxZone.value("minY", 0.0f);
            boxZoneMaxX = boxZone.value("maxX", 0.0f);
            boxZoneMaxY = boxZone.value("maxY", 0.0f);
            boxZoneRequireFull = boxZone.value("requireFull", false);
        }
    }
    
    if (data.contains("requireGlobalValue")) {
        requireGlobalValue = data["requireGlobalValue"].get<bool>();
        if (data.contains("globalValue")) {
            const auto& gv = data["globalValue"];
            globalValueName = gv.value("name", "");
            globalValueComparison = gv.value("comparison", "==");
            globalValueThreshold = gv.value("threshold", 0.0f);
        }
    }

    updateSenseMask();
}

void SensorComponent::initializeDefaults() {
    requireCollision = false;
    requireInteractInput = false;
    maxDistance = 0.0f;
    holdDuration = 0.0f;
    requiredSenses = makeSenseMask(SenseType::None);
    useRegex = false;
    allowSelfTrigger = false;
    useRegexForInstigators = false;
    includeInteractComponent = false;
    targetCacheDirty = true;
    unsatisfiedTargetCacheDirty = true;
    cachedBody = nullptr;
    cachedBodyId = b2_nullBodyId;
    usingSensorFixtures = false;
    shapeCache.clear();
    conditionTimers.clear();
    triggeredInstigators.clear();
    previouslySatisfiedInstigators.clear();
    
    // New sensor type defaults
    requireInputActivity = false;
    inputActivityType = "";
    requireBoxZone = false;
    boxZoneMinX = boxZoneMinY = boxZoneMaxX = boxZoneMaxY = 0.0f;
    boxZoneRequireFull = false;
    requireGlobalValue = false;
    globalValueName = "";
    globalValueComparison = "==";
    globalValueThreshold = 0.0f;
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
    if (requireInputActivity) {
        mask = addSenseToMask(mask, SenseType::InputActivity);
    }
    if (requireBoxZone) {
        mask = addSenseToMask(mask, SenseType::BoxZone);
    }
    if (requireGlobalValue) {
        mask = addSenseToMask(mask, SenseType::GlobalValue);
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
    targetRegexes.clear();
    
    // Compile regex patterns if using regex
    if (useRegex) {
        for (const auto& pattern : targetNames) {
            try {
                targetRegexes.emplace_back(pattern, std::regex::ECMAScript | std::regex::icase);
            } catch (const std::regex_error& e) {
                std::cerr << "[SensorComponent] Invalid regex pattern '" << pattern << "': " << e.what() << std::endl;
            }
        }
    }
    
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
        if (obj == &parent() && !allowSelfTrigger) {
            continue;
        }
        if (targetNames.empty()) {
            continue;
        }
        
        const std::string& objName = obj->getName();
        bool matches = false;
        
        if (useRegex) {
            // Check against regex patterns
            for (const auto& regex : targetRegexes) {
                if (std::regex_search(objName, regex)) {
                    matches = true;
                    break;
                }
            }
        } else {
            // Exact name matching
            if (std::find(targetNames.begin(), targetNames.end(), objName) != targetNames.end()) {
                matches = true;
            }
        }
        
        if (matches) {
            targetCache.push_back(obj);
        }
    }
    targetCacheDirty = false;
}

void SensorComponent::rebuildUnsatisfiedTargetCache() {
    unsatisfiedTargetCache.clear();
    unsatisfiedTargetRegexes.clear();
    
    if (useRegex) {
        for (const auto& pattern : unsatisfiedTargetNames) {
            try {
                unsatisfiedTargetRegexes.emplace_back(pattern, std::regex::ECMAScript | std::regex::icase);
            } catch (const std::regex_error&) {
                // Invalid regex, skip
            }
        }
    }
    
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
        if (obj == &parent() && !allowSelfTrigger) {
            continue;
        }
        if (unsatisfiedTargetNames.empty()) {
            continue;
        }
        
        const std::string& objName = obj->getName();
        bool matches = false;
        
        if (useRegex) {
            // Check against regex patterns
            for (const auto& regex : unsatisfiedTargetRegexes) {
                if (std::regex_search(objName, regex)) {
                    matches = true;
                    break;
                }
            }
        } else {
            // Exact name matching
            if (std::find(unsatisfiedTargetNames.begin(), unsatisfiedTargetNames.end(), objName) != unsatisfiedTargetNames.end()) {
                matches = true;
            }
        }
        
        if (matches) {
            unsatisfiedTargetCache.push_back(obj);
        }
    }
    unsatisfiedTargetCacheDirty = false;
}

void SensorComponent::refreshUnsatisfiedTargetCache() {
    if (!unsatisfiedTargetCacheDirty) {
        for (Object* target : unsatisfiedTargetCache) {
            if (!target || !Object::isAlive(target)) {
                unsatisfiedTargetCacheDirty = true;
                break;
            }
        }
    }

    if (unsatisfiedTargetCacheDirty) {
        rebuildUnsatisfiedTargetCache();
    }
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
            // Filter collision event objects by eligibility
            for (Object* obj : contacting) {
                if (!obj || (obj == &parent() && !allowSelfTrigger) || !Object::isAlive(obj)) {
                    continue;
                }
                // Check eligibility before adding
                bool hasInteract = obj->getComponent<InteractComponent>() != nullptr;
                bool isAllowedInstigator = false;
                
                if (!allowedInstigatorNames.empty()) {
                    const std::string& objName = obj->getName();
                    if (useRegexForInstigators) {
                        for (const auto& pattern : allowedInstigatorNames) {
                            try {
                                std::regex regex(pattern, std::regex::ECMAScript | std::regex::icase);
                                if (std::regex_search(objName, regex)) {
                                    isAllowedInstigator = true;
                                    break;
                                }
                            } catch (const std::regex_error&) {
                                // Invalid regex, skip
                            }
                        }
                    } else {
                        if (std::find(allowedInstigatorNames.begin(), allowedInstigatorNames.end(), objName) != allowedInstigatorNames.end()) {
                            isAllowedInstigator = true;
                        }
                    }
                }
                
                // Respect includeInteractComponent flag
                if (includeInteractComponent) {
                    if (hasInteract || isAllowedInstigator) {
                        uniqueCandidates.insert(obj);
                    }
                } else {
                    if (isAllowedInstigator) {
                        uniqueCandidates.insert(obj);
                    } else if (hasInteract && allowedInstigatorNames.empty()) {
                        // If no allowed list, fall back to InteractComponent (backward compatibility)
                        uniqueCandidates.insert(obj);
                    }
                }
            }
            
            if (usingSensorFixtures) {
                const auto overlapping = eventManager.getSensorOverlappingObjects(shapeId);
                // Filter sensor overlap objects by eligibility
                for (Object* obj : overlapping) {
                    if (!obj || (obj == &parent() && !allowSelfTrigger) || !Object::isAlive(obj)) {
                        continue;
                    }
                    // Check eligibility before adding
                    bool hasInteract = obj->getComponent<InteractComponent>() != nullptr;
                    bool isAllowedInstigator = false;
                    
                    if (!allowedInstigatorNames.empty()) {
                        const std::string& objName = obj->getName();
                        if (useRegexForInstigators) {
                            for (const auto& pattern : allowedInstigatorNames) {
                                try {
                                    std::regex regex(pattern, std::regex::ECMAScript | std::regex::icase);
                                    if (std::regex_search(objName, regex)) {
                                        isAllowedInstigator = true;
                                        break;
                                    }
                                } catch (const std::regex_error&) {
                                    // Invalid regex, skip
                                }
                            }
                        } else {
                            if (std::find(allowedInstigatorNames.begin(), allowedInstigatorNames.end(), objName) != allowedInstigatorNames.end()) {
                                isAllowedInstigator = true;
                            }
                        }
                    }
                    
                    // Respect includeInteractComponent flag
                    if (includeInteractComponent) {
                        if (hasInteract || isAllowedInstigator) {
                            uniqueCandidates.insert(obj);
                        }
                    } else {
                        if (isAllowedInstigator) {
                            uniqueCandidates.insert(obj);
                        } else if (hasInteract && allowedInstigatorNames.empty()) {
                            // If no allowed list, fall back to InteractComponent (backward compatibility)
                            uniqueCandidates.insert(obj);
                        }
                    }
                }
            }
        }
    }

    //if (!requireCollision) {
        Engine* engine = Object::getEngine();
        if (engine) {
            for (const auto& objectPtr : engine->getObjects()) {
                Object* obj = objectPtr.get();
                if (!obj || (obj == &parent() && !allowSelfTrigger)) {
                    continue;
                }
                if (!Object::isAlive(obj)) {
                    continue;
                }
                
                // Check if object is eligible based on includeInteractComponent setting
                bool hasInteract = obj->getComponent<InteractComponent>() != nullptr;
                bool isAllowedInstigator = false;
                
                if (!allowedInstigatorNames.empty()) {
                    const std::string& objName = obj->getName();
                    if (useRegexForInstigators) {
                        for (const auto& pattern : allowedInstigatorNames) {
                            try {
                                std::regex regex(pattern, std::regex::ECMAScript | std::regex::icase);
                                if (std::regex_search(objName, regex)) {
                                    isAllowedInstigator = true;
                                    break;
                                }
                            } catch (const std::regex_error&) {
                                // Invalid regex, skip
                            }
                        }
                    } else {
                        if (std::find(allowedInstigatorNames.begin(), allowedInstigatorNames.end(), objName) != allowedInstigatorNames.end()) {
                            isAllowedInstigator = true;
                        }
                    }
                }
                
                // Respect includeInteractComponent flag
                if (includeInteractComponent) {
                    // Whitelist mode: include objects with InteractComponent OR in allowed list
                    if (hasInteract || isAllowedInstigator) {
                        uniqueCandidates.insert(obj);
                    }
                } else {
                    // Original mode: only include objects in allowed list (bypasses InteractComponent requirement)
                    if (isAllowedInstigator) {
                        uniqueCandidates.insert(obj);
                    } else if (hasInteract && allowedInstigatorNames.empty()) {
                        // If no allowed list, fall back to InteractComponent (backward compatibility)
                        uniqueCandidates.insert(obj);
                    }
                }
            }
        }
    //}

    std::vector<Object*> candidates;
    candidates.reserve(uniqueCandidates.size());
    for (Object* obj : uniqueCandidates) {
        if (obj && (obj != &parent() || allowSelfTrigger) && Object::isAlive(obj)) {
            candidates.push_back(obj);
        }
    }
    return candidates;
}

bool SensorComponent::isInstigatorEligible(Object& instigator) const {
    // Check if instigator is in allowed list
    bool inAllowedList = false;
    if (!allowedInstigatorNames.empty()) {
        const std::string& instigatorName = instigator.getName();
        
        if (useRegexForInstigators) {
            // Check against regex patterns
            for (const auto& pattern : allowedInstigatorNames) {
                try {
                    std::regex regex(pattern, std::regex::ECMAScript | std::regex::icase);
                    if (std::regex_search(instigatorName, regex)) {
                        inAllowedList = true;
                        break;
                    }
                } catch (const std::regex_error&) {
                    // Invalid regex, skip
                }
            }
        } else {
            // Exact name matching
            if (std::find(allowedInstigatorNames.begin(), allowedInstigatorNames.end(), instigatorName) != allowedInstigatorNames.end()) {
                inAllowedList = true;
            }
        }
    }
    
    // Check if instigator has InteractComponent
    auto* interact = instigator.getComponent<InteractComponent>();
    bool hasValidInteract = false;
    if (interact) {
        if (senseMaskHas(requiredSenses, SenseType::Collision) && !interact->supportsSense(SenseType::Collision)) {
            hasValidInteract = false;
        } else if (senseMaskHas(requiredSenses, SenseType::Distance) && !interact->supportsSense(SenseType::Distance)) {
            hasValidInteract = false;
        } else if (senseMaskHas(requiredSenses, SenseType::InteractInput) && !interact->supportsSense(SenseType::InteractInput)) {
            hasValidInteract = false;
        } else {
            hasValidInteract = true;
        }
    }
    
    // If includeInteractComponent is true, allow objects with InteractComponent OR in allowed list (whitelist mode)
    // If includeInteractComponent is false, only allow objects in allowed list (bypass mode)
    if (includeInteractComponent) {
        return inAllowedList || hasValidInteract;
    } else {
        // Original behavior: allowed list bypasses InteractComponent requirement
        if (inAllowedList) {
            return true;  // Allowed instigator, no InteractComponent needed
        }
        return hasValidInteract;  // Otherwise, require InteractComponent
    }
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

bool SensorComponent::verifyInputActivityCondition() const {
    if (!requireInputActivity) {
        return true;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) {
        return false;
    }
    
    if (inputActivityType == "controller_connect" || inputActivityType == "controller_connected") {
        // Check if any controller was recently connected
        // Use a static map to track controller counts per sensor instance
        static std::unordered_map<const SensorComponent*, int> lastControllerCounts;
        InputManager& inputMgr = InputManager::getInstance();
        int currentCount = inputMgr.getNumControllers();
        
        auto it = lastControllerCounts.find(this);
        if (it == lastControllerCounts.end()) {
            // First time checking - initialize
            lastControllerCounts[this] = currentCount;
            return false;  // Don't trigger on first check
        }
        
        int lastCount = it->second;
        if (currentCount > lastCount) {
            lastControllerCounts[this] = currentCount;
            return true;  // Controller was added
        }
        
        // Update count even if no change (for next frame)
        lastControllerCounts[this] = currentCount;
        return false;
    } else if (inputActivityType == "client_join" || inputActivityType == "client_joined") {
        // Check if a client joined the hosted game
        // Note: This is a simplified check. For proper event-based detection,
        // you would need to add event tracking in HostManager
        auto hostMgr = engine->getHostManager();
        if (hostMgr && hostMgr->IsHosting()) {
            // Placeholder: In a full implementation, you'd track client join events
            // For now, this always returns false
            // TODO: Implement proper client join event tracking
            return false;
        }
        return false;
    }
    
    return false;
}

bool SensorComponent::verifyBoxZoneCondition(Object& instigator) const {
    if (!requireBoxZone) {
        return true;
    }
    
    auto* body = instigator.getComponent<BodyComponent>();
    if (!body) {
        return false;
    }
    
    float x, y, angle;
    std::tie(x, y, angle) = body->getPosition();
    (void)angle;
    
    if (boxZoneRequireFull) {
        // Object must be fully inside the box
        // This is simplified - in reality you'd check the object's bounds
        return (x >= boxZoneMinX && x <= boxZoneMaxX && 
                y >= boxZoneMinY && y <= boxZoneMaxY);
    } else {
        // Object is partially inside if any part overlaps
        // Simplified check - just check center point
        return (x >= boxZoneMinX && x <= boxZoneMaxX && 
                y >= boxZoneMinY && y <= boxZoneMaxY);
    }
}

bool SensorComponent::verifyGlobalValueCondition() const {
    if (!requireGlobalValue || globalValueName.empty()) {
        return true;
    }
    
    GlobalValueManager& gvm = GlobalValueManager::getInstance();
    float value = gvm.getValue(globalValueName);
    
    if (globalValueComparison == ">") {
        return value > globalValueThreshold;
    } else if (globalValueComparison == ">=") {
        return value >= globalValueThreshold;
    } else if (globalValueComparison == "==" || globalValueComparison == "=") {
        return std::abs(value - globalValueThreshold) < 0.0001f;  // Float comparison
    } else if (globalValueComparison == "<=") {
        return value <= globalValueThreshold;
    } else if (globalValueComparison == "<") {
        return value < globalValueThreshold;
    } else if (globalValueComparison == "!=" || globalValueComparison == "<>") {
        return std::abs(value - globalValueThreshold) >= 0.0001f;
    }
    
    return false;
}

void SensorComponent::cleanExpiredTimers(const std::unordered_set<Object*>& processed) {
    for (auto it = conditionTimers.begin(); it != conditionTimers.end();) {
        Object* instigator = it->first;
        if (!instigator || !Object::isAlive(instigator) || processed.find(instigator) == processed.end()) {
            it = conditionTimers.erase(it);
            triggeredInstigators.erase(instigator);
        } else {
            ++it;
        }
    }
}

void SensorComponent::advanceTimersForCandidates(const std::vector<Object*>& candidates, float deltaTime) {
    std::unordered_set<Object*> processed;

    // Check non-object-based conditions first (InputActivity, GlobalValue)
    bool inputActivityOk = verifyInputActivityCondition();
    bool globalValueOk = verifyGlobalValueCondition();
    
    // If we only have non-object conditions, we can trigger without a candidate
    // Use a dummy instigator (the sensor itself) when there are no object-based conditions
    bool hasOnlyNonObjectConditions = (senseMaskHas(requiredSenses, SenseType::InputActivity) || 
                                        senseMaskHas(requiredSenses, SenseType::GlobalValue)) &&
                                       !requireCollision && maxDistance <= 0.0f && 
                                       !requireInteractInput && !requireBoxZone;
    
    if (hasOnlyNonObjectConditions) {
        if (inputActivityOk && globalValueOk) {
            // Only non-object conditions, trigger with self as instigator
            // Check if already triggered for this instigator (per-instigator satisfaction)
            if (triggeredInstigators.find(&parent()) == triggeredInstigators.end()) {
                float nextTimer = 0.0f;
                auto existing = conditionTimers.find(&parent());
                if (existing != conditionTimers.end()) {
                    nextTimer = existing->second;
                }
                nextTimer += deltaTime;
                conditionTimers[&parent()] = nextTimer;
                
                const float requiredHold = clampNonNegative(holdDuration);
                if (nextTimer >= requiredHold) {
                    trigger(parent());
                    triggeredInstigators.insert(&parent());
                    previouslySatisfiedInstigators.insert(&parent());  // Mark as previously satisfied
                }
            }
            processed.insert(&parent());
        } else {
            // Conditions not met - check if system was previously satisfied
            bool wasSystemSatisfied = previouslySatisfiedInstigators.find(&parent()) != previouslySatisfiedInstigators.end();
            
            // Reset timer and allow retrigger
            conditionTimers[&parent()] = 0.0f;
            triggeredInstigators.erase(&parent());
            previouslySatisfiedInstigators.erase(&parent());
            
            // If system was previously satisfied and now isn't, trigger unsatisfied targets
            if (wasSystemSatisfied && !unsatisfiedTargetNames.empty()) {
                triggerUnsatisfied(parent());
            }
        }
    }
    
    // Special handling: If sensor has GlobalValue or InputActivity condition,
    // these should only trigger once (not once per candidate)
    // Use the sensor itself as a special "system" instigator for these conditions
    bool hasSystemConditions = senseMaskHas(requiredSenses, SenseType::InputActivity) || 
                               senseMaskHas(requiredSenses, SenseType::GlobalValue);
    Object* systemInstigator = &parent();  // Use sensor itself as system instigator
    
    // Track if system conditions have already triggered
    bool systemConditionsTriggered = triggeredInstigators.find(systemInstigator) != triggeredInstigators.end();
    bool systemConditionsMet = inputActivityOk && globalValueOk;

    for (Object* candidate : candidates) {
        if (!candidate || (candidate == &parent() && !allowSelfTrigger) || !Object::isAlive(candidate)) {
            continue;
        }

        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact && requireInteractInput && allowedInstigatorNames.empty()) {
            conditionTimers.erase(candidate);
            continue;
        }

        // Always check if instigator is eligible (unless it's a pure system condition sensor)
        // BoxZone sensors still need to check eligibility
        bool isPureSystemSensor = (senseMaskHas(requiredSenses, SenseType::InputActivity) || 
                                   senseMaskHas(requiredSenses, SenseType::GlobalValue)) &&
                                  !requireCollision && maxDistance <= 0.0f && 
                                  !requireInteractInput && !requireBoxZone;
        
        if (!isPureSystemSensor && !isInstigatorEligible(*candidate)) {
            conditionTimers.erase(candidate);
            continue;
        }

        const bool collisionOk = verifyCollisionCondition(*candidate);
        const bool distanceOk = verifyDistanceCondition(*candidate);
        const bool interactOk = interact ? verifyInteractCondition(*interact) : (!requireInteractInput);
        const bool boxZoneOk = verifyBoxZoneCondition(*candidate);

        processed.insert(candidate);

        // For object-specific conditions (collision, distance, interact, boxzone)
        const bool objectConditionsMet = collisionOk && distanceOk && interactOk && boxZoneOk;
        
        // If we have system conditions (GlobalValue/InputActivity), check if they're met
        // and if they've already triggered
        if (hasSystemConditions) {
            // System conditions must be met AND not already triggered
            if (!systemConditionsMet || systemConditionsTriggered) {
                // System conditions not met or already triggered, skip this candidate
                conditionTimers[candidate] = 0.0f;
                triggeredInstigators.erase(candidate);
                continue;
            }
        }

        const bool conditionsMet = objectConditionsMet && systemConditionsMet;
        if (conditionsMet) {
            // If we have system conditions and they haven't triggered yet, trigger once with first valid candidate
            if (hasSystemConditions && !systemConditionsTriggered) {
                // Check if this candidate's object conditions are met
                if (objectConditionsMet) {
                    // Use this candidate as the instigator, but mark system conditions as triggered
                    if (triggeredInstigators.find(candidate) == triggeredInstigators.end()) {
                        float nextTimer = 0.0f;
                        auto existing = conditionTimers.find(candidate);
                        if (existing != conditionTimers.end()) {
                            nextTimer = existing->second;
                        }
                        nextTimer += deltaTime;
                        conditionTimers[candidate] = nextTimer;

                        const float requiredHold = clampNonNegative(holdDuration);
                        if (nextTimer >= requiredHold) {
                            trigger(*candidate);
                            triggeredInstigators.insert(candidate);
                            previouslySatisfiedInstigators.insert(candidate);  // Mark as previously satisfied
                            triggeredInstigators.insert(systemInstigator);  // Mark system conditions as triggered
                            systemConditionsTriggered = true;  // Update local flag
                        }
                    }
                }
            } else if (!hasSystemConditions) {
                // No system conditions, use per-candidate logic
                if (triggeredInstigators.find(candidate) == triggeredInstigators.end()) {
                    float nextTimer = 0.0f;
                    auto existing = conditionTimers.find(candidate);
                    if (existing != conditionTimers.end()) {
                        nextTimer = existing->second;
                    }
                    nextTimer += deltaTime;
                    conditionTimers[candidate] = nextTimer;

                    const float requiredHold = clampNonNegative(holdDuration);
                    if (nextTimer >= requiredHold) {
                        trigger(*candidate);
                        triggeredInstigators.insert(candidate);
                        previouslySatisfiedInstigators.insert(candidate);  // Mark as previously satisfied
                    }
                }
            }
        } else {
            // Conditions not met - check if this instigator was previously satisfied
            bool wasPreviouslySatisfied = previouslySatisfiedInstigators.find(candidate) != previouslySatisfiedInstigators.end();
            
            // Reset timer and allow retrigger for this instigator
            conditionTimers[candidate] = 0.0f;
            triggeredInstigators.erase(candidate);
            previouslySatisfiedInstigators.erase(candidate);
            
            // If instigator was previously satisfied and now isn't, trigger unsatisfied targets
            if (wasPreviouslySatisfied && !unsatisfiedTargetNames.empty()) {
                triggerUnsatisfied(*candidate);
            }
        }
    }
    
    // Reset system conditions trigger if they're no longer met (outside candidate loop)
    if (hasSystemConditions && !systemConditionsMet) {
        bool wasSystemSatisfied = triggeredInstigators.find(systemInstigator) != triggeredInstigators.end();
        triggeredInstigators.erase(systemInstigator);
        previouslySatisfiedInstigators.erase(systemInstigator);
        
        // If system conditions were previously satisfied and now aren't, trigger unsatisfied targets
        if (wasSystemSatisfied && !unsatisfiedTargetNames.empty()) {
            triggerUnsatisfied(parent());
        }
    }

    cleanExpiredTimers(processed);
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
        // Allow self-triggering if enabled
        if (target == &parent() && !allowSelfTrigger) {
            continue;
        }
        target->use(instigator);
    }
}

void SensorComponent::triggerUnsatisfied(Object& instigator) {
    refreshUnsatisfiedTargetCache();

    const std::string sensorName = parent().getName().empty() ? "<unnamed_sensor>" : parent().getName();
    const std::string instigatorName = instigator.getName().empty() ? "<unnamed_instigator>" : instigator.getName();
    std::cout << "[SensorComponent] Sensor '" << sensorName << "' unsatisfied by '" << instigatorName << "'. Unsatisfied target count: "
              << unsatisfiedTargetCache.size() << std::endl;

    if (unsatisfiedTargetCache.empty()) {
        return;
    }

    for (Object* target : unsatisfiedTargetCache) {
        if (!target || !Object::isAlive(target)) {
            unsatisfiedTargetCacheDirty = true;
            continue;
        }
        // Allow self-triggering if enabled
        if (target == &parent() && !allowSelfTrigger) {
            continue;
        }
        target->use(instigator);
    }
}

void SensorComponent::update(float deltaTime) {
    updateSenseMask();
    refreshShapeCache();
    refreshTargetCache();
    refreshUnsatisfiedTargetCache();

    if (requireCollision && shapeCache.empty()) {
        // Without shapes we can't evaluate collision; ensure timers are cleared.
        conditionTimers.clear();
        triggeredInstigators.clear();
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
    if (useRegex) {
        data["useRegex"] = useRegex;
    }
    if (allowSelfTrigger) {
        data["allowSelfTrigger"] = allowSelfTrigger;
    }
    if (!targetNames.empty()) {
        data["targetObjects"] = targetNames;
    }
    if (!unsatisfiedTargetNames.empty()) {
        data["unsatisfiedTargetObjects"] = unsatisfiedTargetNames;
    }
    if (includeInteractComponent) {
        data["includeInteractComponent"] = includeInteractComponent;
    }
    if (!allowedInstigatorNames.empty()) {
        data["allowedInstigators"] = allowedInstigatorNames;
        if (useRegexForInstigators) {
            data["useRegexForInstigators"] = useRegexForInstigators;
        }
    }
    
    // New sensor type configurations
    if (requireInputActivity) {
        data["requireInputActivity"] = requireInputActivity;
        if (!inputActivityType.empty()) {
            data["inputActivityType"] = inputActivityType;
        }
    }
    
    if (requireBoxZone) {
        data["requireBoxZone"] = requireBoxZone;
        nlohmann::json boxZone;
        boxZone["minX"] = boxZoneMinX;
        boxZone["minY"] = boxZoneMinY;
        boxZone["maxX"] = boxZoneMaxX;
        boxZone["maxY"] = boxZoneMaxY;
        boxZone["requireFull"] = boxZoneRequireFull;
        data["boxZone"] = boxZone;
    }
    
    if (requireGlobalValue) {
        data["requireGlobalValue"] = requireGlobalValue;
        nlohmann::json gv;
        gv["name"] = globalValueName;
        gv["comparison"] = globalValueComparison;
        gv["threshold"] = globalValueThreshold;
        data["globalValue"] = gv;
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
    if (requireInputActivity) {
        ++count;
    }
    if (requireBoxZone) {
        ++count;
    }
    if (requireGlobalValue) {
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
    bool boxZoneSatisfied = !requireBoxZone; // If not required, doesn't need to be satisfied
    
    // Check non-object-based conditions
    bool inputActivitySatisfied = verifyInputActivityCondition();
    bool globalValueSatisfied = verifyGlobalValueCondition();
    
    // Check each candidate to see if it satisfies conditions
    for (Object* candidate : candidates) {
        if (!candidate || (candidate == &parent() && !allowSelfTrigger) || !Object::isAlive(candidate)) {
            continue;
        }
        
        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact && requireInteractInput) {
            continue;
        }
        
        if (interact && !isInstigatorEligible(*candidate) && 
            !senseMaskHas(requiredSenses, SenseType::InputActivity) && 
            !senseMaskHas(requiredSenses, SenseType::GlobalValue) && 
            !senseMaskHas(requiredSenses, SenseType::BoxZone)) {
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
        if (requireInteractInput && !interactSatisfied && interact) {
            if (verifyInteractCondition(*interact)) {
                interactSatisfied = true;
            }
        }
        
        // Check box zone condition if required and not yet satisfied
        if (requireBoxZone && !boxZoneSatisfied) {
            if (verifyBoxZoneCondition(*candidate)) {
                boxZoneSatisfied = true;
            }
        }
        
        // Early exit if all required conditions are satisfied
        if (collisionSatisfied && distanceSatisfied && interactSatisfied && 
            boxZoneSatisfied && inputActivitySatisfied && globalValueSatisfied) {
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
    if (requireInputActivity && inputActivitySatisfied) {
        ++satisfied;
    }
    if (requireBoxZone && boxZoneSatisfied) {
        ++satisfied;
    }
    if (requireGlobalValue && globalValueSatisfied) {
        ++satisfied;
    }
    
    return satisfied;
}

int SensorComponent::getSatisfyingObjectCount(const std::vector<Object*>& allObjects) const {
    int count = 0;
    
    // Get candidates that might satisfy conditions
    std::vector<Object*> candidates = gatherCandidates();
    
    // Check non-object-based conditions (these apply to all candidates)
    bool inputActivityOk = verifyInputActivityCondition();
    bool globalValueOk = verifyGlobalValueCondition();
    
    // Check each candidate to see if it satisfies all conditions
    for (Object* candidate : candidates) {
        if (!candidate || (candidate == &parent() && !allowSelfTrigger) || !Object::isAlive(candidate)) {
            continue;
        }
        
        // Check eligibility
        if (!isInstigatorEligible(*candidate)) {
            continue;
        }
        
        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact && requireInteractInput && allowedInstigatorNames.empty()) {
            continue;
        }
        
        // Check all object-specific conditions
        const bool collisionOk = verifyCollisionCondition(*candidate);
        const bool distanceOk = verifyDistanceCondition(*candidate);
        const bool interactOk = interact ? verifyInteractCondition(*interact) : (!requireInteractInput);
        const bool boxZoneOk = verifyBoxZoneCondition(*candidate);
        
        // Check if all conditions are met
        const bool allConditionsMet = collisionOk && distanceOk && interactOk && boxZoneOk && 
                                      inputActivityOk && globalValueOk;
        
        if (allConditionsMet) {
            ++count;
        }
    }
    
    return count;
}

std::vector<Object*> SensorComponent::getTargetObjects(const std::vector<std::unique_ptr<Object>>& allObjects) const {
    std::vector<Object*> targets;
    if (targetNames.empty()) {
        return targets;
    }
    
    // Compile regex patterns if using regex
    std::vector<std::regex> regexes;
    if (useRegex) {
        for (const auto& pattern : targetNames) {
            try {
                regexes.emplace_back(pattern, std::regex::ECMAScript | std::regex::icase);
            } catch (const std::regex_error& e) {
                std::cerr << "[SensorComponent] Invalid regex pattern '" << pattern << "': " << e.what() << std::endl;
            }
        }
    }
    
    for (const auto& objectPtr : allObjects) {
        if (!objectPtr) {
            continue;
        }
        Object* obj = objectPtr.get();
        if (!obj || !Object::isAlive(obj)) {
            continue;
        }
        if (obj == &parent() && !allowSelfTrigger) {
            continue;
        }
        const std::string& objName = obj->getName();
        
        bool matches = false;
        if (useRegex) {
            // Check against regex patterns
            for (const auto& regex : regexes) {
                if (std::regex_search(objName, regex)) {
                    matches = true;
                    break;
                }
            }
        } else {
            // Exact name matching
            if (std::find(targetNames.begin(), targetNames.end(), objName) != targetNames.end()) {
                matches = true;
            }
        }
        
        if (matches) {
            targets.push_back(obj);
        }
    }
    
    return targets;
}

std::vector<Object*> SensorComponent::getSatisfiedObjects() const {
    std::vector<Object*> satisfied;
    std::vector<Object*> candidates = gatherCandidates();

    bool inputActivityOk = verifyInputActivityCondition();
    bool globalValueOk = verifyGlobalValueCondition();

    for (Object* candidate : candidates) {
        if (!candidate || (candidate == &parent() && !allowSelfTrigger) || !Object::isAlive(candidate)) {
            continue;
        }

        if (!isInstigatorEligible(*candidate)) {
            continue;
        }

        auto* interact = candidate->getComponent<InteractComponent>();
        if (!interact && requireInteractInput && allowedInstigatorNames.empty()) {
            continue;
        }

        const bool collisionOk = verifyCollisionCondition(*candidate);
        const bool distanceOk = verifyDistanceCondition(*candidate);
        const bool interactOk = interact ? verifyInteractCondition(*interact) : (!requireInteractInput);
        const bool boxZoneOk = verifyBoxZoneCondition(*candidate);

        const bool allConditionsMet = collisionOk && distanceOk && interactOk && boxZoneOk &&
                                      inputActivityOk && globalValueOk;
        if (allConditionsMet) {
            satisfied.push_back(candidate);
        }
    }

    return satisfied;
}

std::vector<Object*> SensorComponent::getSatisfiedInstigators() const {
    std::vector<Object*> instigators;
    instigators.reserve(triggeredInstigators.size());
    for (Object* instigator : triggeredInstigators) {
        if (!instigator) {
            continue;
        }
        if (instigator == &parent()) {
            continue;
        }
        if (!Object::isAlive(instigator)) {
            continue;
        }
        instigators.push_back(instigator);
    }
    return instigators;
}

bool SensorComponent::isInstigatorSatisfied(const Object* object) const {
    if (!object || object == &parent()) {
        return false;
    }
    return triggeredInstigators.find(const_cast<Object*>(object)) != triggeredInstigators.end();
}

static ComponentRegistrar<SensorComponent> registrar("SensorComponent");

