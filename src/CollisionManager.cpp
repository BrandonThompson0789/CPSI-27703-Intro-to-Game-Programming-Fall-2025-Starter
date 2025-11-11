#include "CollisionManager.h"

#include "Engine.h"
#include "Object.h"
#include "components/BodyComponent.h"
#include "components/CollisionDamageComponent.h"
#include "components/HealthComponent.h"
#include "SoundManager.h"

#include <algorithm>

CollisionManager::CollisionManager(Engine& engineRef)
    : engine(engineRef), worldId(b2_nullWorldId) {
}

void CollisionManager::setWorld(b2WorldId world) {
    worldId = world;
}

void CollisionManager::gatherCollisions() {
    impacts.clear();

    if (B2_IS_NULL(worldId)) {
        return;
    }

    const b2ContactEvents events = b2World_GetContactEvents(worldId);
    for (int i = 0; i < events.hitCount; ++i) {
        handleHitEvent(events.hitEvents[i]);
    }
}

void CollisionManager::processCollisions(float /*deltaTime*/) {
    for (const auto& impact : impacts) {
        applyImpactDamage(impact);
        SoundManager::getInstance().playImpactSound(impact);
    }
}

void CollisionManager::clearImpacts() {
    impacts.clear();
}

void CollisionManager::handleHitEvent(const b2ContactHitEvent& hitEvent) {
    Object* objectA = getObjectFromShape(hitEvent.shapeIdA);
    Object* objectB = getObjectFromShape(hitEvent.shapeIdB);

    if (objectA == nullptr || objectB == nullptr || objectA == objectB) {
        return;
    }

    CollisionImpact impact;
    impact.objectA = objectA;
    impact.objectB = objectB;
    impact.shapeIdA = hitEvent.shapeIdA;
    impact.shapeIdB = hitEvent.shapeIdB;
    impact.materialIdA = b2Shape_GetMaterial(hitEvent.shapeIdA);
    impact.materialIdB = b2Shape_GetMaterial(hitEvent.shapeIdB);
    impact.approachSpeed = hitEvent.approachSpeed;
    impact.point = hitEvent.point;
    impact.normal = hitEvent.normal;

    impacts.push_back(impact);
}

Object* CollisionManager::getObjectFromShape(b2ShapeId shapeId) const {
    if (B2_IS_NULL(shapeId)) {
        return nullptr;
    }

    if (void* userData = b2Shape_GetUserData(shapeId)) {
        return static_cast<Object*>(userData);
    }

    b2BodyId bodyId = b2Shape_GetBody(shapeId);
    if (B2_IS_NULL(bodyId)) {
        return nullptr;
    }

    if (void* bodyUserData = b2Body_GetUserData(bodyId)) {
        return static_cast<Object*>(bodyUserData);
    }

    return nullptr;
}

namespace {

b2BodyType getBodyType(const Object* object) {
    if (!object) {
        return b2_staticBody;
    }
    const auto* bodyComponent = object->getComponent<BodyComponent>();
    if (!bodyComponent) {
        return b2_staticBody;
    }

    const b2BodyId bodyId = bodyComponent->getBodyId();
    if (B2_IS_NULL(bodyId)) {
        return b2_staticBody;
    }

    return b2Body_GetType(bodyId);
}

float getBodyMass(b2ShapeId shapeId) {
    const b2BodyId bodyId = b2Shape_GetBody(shapeId);
    if (B2_IS_NULL(bodyId)) {
        return 0.0f;
    }
    return b2Body_GetMass(bodyId);
}

} // namespace

void CollisionManager::applyImpactDamage(const CollisionImpact& impact) {
    auto* healthA = impact.objectA ? impact.objectA->getComponent<HealthComponent>() : nullptr;
    auto* healthB = impact.objectB ? impact.objectB->getComponent<HealthComponent>() : nullptr;
    auto* damageA = impact.objectA ? impact.objectA->getComponent<CollisionDamageComponent>() : nullptr;
    auto* damageB = impact.objectB ? impact.objectB->getComponent<CollisionDamageComponent>() : nullptr;

    if (!healthA && !healthB) {
        return;
    }

    const b2BodyType typeA = getBodyType(impact.objectA);
    const b2BodyType typeB = getBodyType(impact.objectB);
    const float massA = getBodyMass(impact.shapeIdA);
    const float massB = getBodyMass(impact.shapeIdB);

    if (healthA && healthA->receivesCollisionDamage() && damageB && damageB->canAffectBodyType(typeA)) {
        const float damageToA = damageB->calculateDamage(*impact.objectA, massB, impact.approachSpeed);
        if (damageToA > 0.0f) {
            healthA->applyImpactDamage(damageToA, impact.approachSpeed, impact.materialIdB, impact.objectB);
        }
    }

    if (healthB && healthB->receivesCollisionDamage() && damageA && damageA->canAffectBodyType(typeB)) {
        const float damageToB = damageA->calculateDamage(*impact.objectB, massA, impact.approachSpeed);
        if (damageToB > 0.0f) {
            healthB->applyImpactDamage(damageToB, impact.approachSpeed, impact.materialIdA, impact.objectA);
        }
    }
}

