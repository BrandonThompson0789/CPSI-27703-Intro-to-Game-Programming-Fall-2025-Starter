#pragma once

#include <vector>

#include <box2d/box2d.h>

class Object;
class Engine;

struct CollisionImpact {
    Object* objectA = nullptr;
    Object* objectB = nullptr;
    b2ShapeId shapeIdA = b2_nullShapeId;
    b2ShapeId shapeIdB = b2_nullShapeId;
    int materialIdA = 0;
    int materialIdB = 0;
    float approachSpeed = 0.0f;
    b2Vec2 point{0.0f, 0.0f};
    b2Vec2 normal{0.0f, 0.0f};
};

class CollisionManager {
public:
    explicit CollisionManager(Engine& engine);

    void setWorld(b2WorldId world);
    void gatherCollisions();
    void processCollisions(float deltaTime);

    const std::vector<CollisionImpact>& getImpacts() const { return impacts; }
    void clearImpacts();

private:
    Object* getObjectFromShape(b2ShapeId shapeId) const;
    void handleHitEvent(const b2ContactHitEvent& hitEvent);
    void applyImpactDamage(const CollisionImpact& impact);

    Engine& engine;
    b2WorldId worldId;
    std::vector<CollisionImpact> impacts;
};

