#include "SensorEventManager.h"

#include "Object.h"

#include <algorithm>

SensorEventManager& SensorEventManager::getInstance() {
    static SensorEventManager instance;
    return instance;
}

void SensorEventManager::processWorldEvents(b2WorldId worldId) {
    if (B2_IS_NULL(worldId)) {
        clear();
        return;
    }

    pruneInvalidEntries(contactTouches);
    pruneInvalidEntries(sensorTouches);

    const b2ContactEvents contactEvents = b2World_GetContactEvents(worldId);
    for (int i = 0; i < contactEvents.beginCount; ++i) {
        const b2ContactBeginTouchEvent& evt = contactEvents.beginEvents[i];
        Object* objectA = getObjectFromShape(evt.shapeIdA);
        Object* objectB = getObjectFromShape(evt.shapeIdB);
        if (objectA && objectB && objectA != objectB) {
            addTouch(contactTouches, evt.shapeIdA, objectB);
            addTouch(contactTouches, evt.shapeIdB, objectA);
        }
    }

    for (int i = 0; i < contactEvents.endCount; ++i) {
        const b2ContactEndTouchEvent& evt = contactEvents.endEvents[i];
        Object* objectA = getObjectFromShape(evt.shapeIdA);
        Object* objectB = getObjectFromShape(evt.shapeIdB);
        if (objectA && objectB && objectA != objectB) {
            removeTouch(contactTouches, evt.shapeIdA, objectB);
            removeTouch(contactTouches, evt.shapeIdB, objectA);
        }
    }

    const b2SensorEvents sensorEvents = b2World_GetSensorEvents(worldId);
    for (int i = 0; i < sensorEvents.beginCount; ++i) {
        const b2SensorBeginTouchEvent& evt = sensorEvents.beginEvents[i];
        Object* sensorObject = getObjectFromShape(evt.sensorShapeId);
        Object* visitorObject = getObjectFromShape(evt.visitorShapeId);
        if (sensorObject && visitorObject && sensorObject != visitorObject) {
            addTouch(sensorTouches, evt.sensorShapeId, visitorObject);
            addTouch(sensorTouches, evt.visitorShapeId, sensorObject);
        }
    }

    for (int i = 0; i < sensorEvents.endCount; ++i) {
        const b2SensorEndTouchEvent& evt = sensorEvents.endEvents[i];
        Object* sensorObject = getObjectFromShape(evt.sensorShapeId);
        Object* visitorObject = getObjectFromShape(evt.visitorShapeId);
        removeTouch(sensorTouches, evt.sensorShapeId, visitorObject);
        removeTouch(sensorTouches, evt.visitorShapeId, sensorObject);
    }
}

std::vector<Object*> SensorEventManager::getContactingObjects(b2ShapeId shapeId) const {
    std::vector<Object*> results;
    if (B2_IS_NULL(shapeId)) {
        return results;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto it = contactTouches.find(key);
    if (it == contactTouches.end()) {
        return results;
    }
    results.reserve(it->second.size());
    for (const auto& [object, count] : it->second) {
        if (object && count > 0 && Object::isAlive(object)) {
            results.push_back(object);
        }
    }
    return results;
}

std::vector<Object*> SensorEventManager::getSensorOverlappingObjects(b2ShapeId shapeId) const {
    std::vector<Object*> results;
    if (B2_IS_NULL(shapeId)) {
        return results;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto it = sensorTouches.find(key);
    if (it == sensorTouches.end()) {
        return results;
    }
    results.reserve(it->second.size());
    for (const auto& [object, count] : it->second) {
        if (object && count > 0 && Object::isAlive(object)) {
            results.push_back(object);
        }
    }
    return results;
}

bool SensorEventManager::hasContactWith(b2ShapeId shapeId, const Object* other) const {
    if (!other || B2_IS_NULL(shapeId)) {
        return false;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto it = contactTouches.find(key);
    if (it == contactTouches.end()) {
        return false;
    }
    auto target = it->second.find(const_cast<Object*>(other));
    return target != it->second.end() && target->second > 0 && Object::isAlive(other);
}

bool SensorEventManager::hasSensorOverlapWith(b2ShapeId shapeId, const Object* other) const {
    if (!other || B2_IS_NULL(shapeId)) {
        return false;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto it = sensorTouches.find(key);
    if (it == sensorTouches.end()) {
        return false;
    }
    auto target = it->second.find(const_cast<Object*>(other));
    return target != it->second.end() && target->second > 0 && Object::isAlive(other);
}

void SensorEventManager::clear() {
    contactTouches.clear();
    sensorTouches.clear();
}

uint64_t SensorEventManager::storeShapeId(b2ShapeId shapeId) {
    return b2StoreShapeId(shapeId);
}

Object* SensorEventManager::getObjectFromShape(b2ShapeId shapeId) {
    if (B2_IS_NULL(shapeId)) {
        return nullptr;
    }
    // Check if shape is still valid before accessing it (prevents crash when object dies)
    if (!b2Shape_IsValid(shapeId)) {
        return nullptr;
    }
    if (void* userData = b2Shape_GetUserData(shapeId)) {
        Object* object = static_cast<Object*>(userData);
        if (object && Object::isAlive(object)) {
            return object;
        }
    }
    b2BodyId bodyId = b2Shape_GetBody(shapeId);
    if (B2_IS_NULL(bodyId)) {
        return nullptr;
    }
    if (void* bodyData = b2Body_GetUserData(bodyId)) {
        Object* object = static_cast<Object*>(bodyData);
        if (object && Object::isAlive(object)) {
            return object;
        }
    }
    return nullptr;
}

void SensorEventManager::addTouch(ShapeTouchMap& map, b2ShapeId shapeId, Object* other) {
    if (B2_IS_NULL(shapeId) || other == nullptr) {
        return;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto& entry = map[key];
    entry[other] += 1;
}

void SensorEventManager::removeTouch(ShapeTouchMap& map, b2ShapeId shapeId, Object* other) {
    if (B2_IS_NULL(shapeId)) {
        return;
    }
    const uint64_t key = storeShapeId(shapeId);
    auto it = map.find(key);
    if (it == map.end()) {
        return;
    }

    if (other == nullptr) {
        map.erase(it);
        return;
    }

    auto objectIt = it->second.find(other);
    if (objectIt == it->second.end()) {
        return;
    }

    objectIt->second = std::max(0, objectIt->second - 1);
    if (objectIt->second == 0) {
        it->second.erase(objectIt);
    }

    if (it->second.empty()) {
        map.erase(it);
    }
}

void SensorEventManager::pruneInvalidEntries(ShapeTouchMap& map) {
    std::vector<uint64_t> shapesToRemove;
    shapesToRemove.reserve(map.size());
    for (auto& [key, touches] : map) {
        b2ShapeId shapeId = b2LoadShapeId(key);
        if (!b2Shape_IsValid(shapeId)) {
            shapesToRemove.push_back(key);
            continue;
        }
        for (auto it = touches.begin(); it != touches.end(); ) {
            if (!it->first || !Object::isAlive(it->first) || it->second <= 0) {
                it = touches.erase(it);
            } else {
                ++it;
            }
        }
        if (touches.empty()) {
            shapesToRemove.push_back(key);
        }
    }

    for (uint64_t key : shapesToRemove) {
        map.erase(key);
    }
}

