#pragma once

#include <box2d/box2d.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class Object;

class SensorEventManager {
public:
    static SensorEventManager& getInstance();

    void processWorldEvents(b2WorldId worldId);

    std::vector<Object*> getContactingObjects(b2ShapeId shapeId) const;
    std::vector<Object*> getSensorOverlappingObjects(b2ShapeId shapeId) const;
    bool hasContactWith(b2ShapeId shapeId, const Object* other) const;
    bool hasSensorOverlapWith(b2ShapeId shapeId, const Object* other) const;

    void clear();

private:
    using TouchMap = std::unordered_map<Object*, int>;
    using ShapeTouchMap = std::unordered_map<uint64_t, TouchMap>;

    ShapeTouchMap contactTouches;
    ShapeTouchMap sensorTouches;

    static uint64_t storeShapeId(b2ShapeId shapeId);
    static Object* getObjectFromShape(b2ShapeId shapeId);

    static void addTouch(ShapeTouchMap& map, b2ShapeId shapeId, Object* other);
    static void removeTouch(ShapeTouchMap& map, b2ShapeId shapeId, Object* other);
    static void pruneInvalidEntries(ShapeTouchMap& map);
};


