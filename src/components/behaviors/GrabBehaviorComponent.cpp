#include "GrabBehaviorComponent.h"
#include "../ComponentLibrary.h"
#include "../JointComponent.h"
#include "../SoundComponent.h"
#include "../../Engine.h"
#include "../../Object.h"
#include <box2d/box2d.h>
#include <cmath>
#include <iostream>
#include <vector>

GrabBehaviorComponent::GrabBehaviorComponent(Object& parent)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , grabbedObject(nullptr)
    , grabJoint(nullptr)
    , sound(nullptr)
    , grabDistance(80.0f)
    , grabForce(200.0f)
    , breakForce(500.0f)
    , maxGrabDensity(-1.0f)
    , wasInteractPressed(false) {
    resolveDependencies();
}

GrabBehaviorComponent::GrabBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , grabbedObject(nullptr)
    , grabJoint(nullptr)
    , sound(nullptr)
    , grabDistance(data.value("grabDistance", 80.0f))
    , grabForce(data.value("grabForce", 200.0f))
    , breakForce(data.value("breakForce", 500.0f))
    , maxGrabDensity(data.value("maxGrabDensity", -1.0f))
    , wasInteractPressed(false) {
    resolveDependencies();
}

void GrabBehaviorComponent::resolveDependencies() {
    input = parent().getComponent<InputComponent>();
    body = parent().getComponent<BodyComponent>();
    grabJoint = parent().getComponent<JointComponent>();
    sound = parent().getComponent<SoundComponent>();

    if (!input) {
        std::cerr << "Warning: GrabBehaviorComponent requires InputComponent!\n";
    }
    if (!body) {
        std::cerr << "Warning: GrabBehaviorComponent requires BodyComponent!\n";
    }

    // Ensure we have a JointComponent available ahead of time to avoid modifying
    // the component list while updating (which can invalidate iterators).
    if (!grabJoint) {
        grabJoint = parent().addComponent<JointComponent>();
    }
}

nlohmann::json GrabBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["grabDistance"] = grabDistance;
    j["grabForce"] = grabForce;
    j["breakForce"] = breakForce;
    if (maxGrabDensity >= 0.0f) {
        j["maxGrabDensity"] = maxGrabDensity;
    }
    return j;
}

void GrabBehaviorComponent::update(float deltaTime) {
    (void)deltaTime;

    if (!input || !body) {
        return;
    }

    if (!sound) {
        sound = parent().getComponent<SoundComponent>();
    }

    autoReleaseIfNecessary();
    maintainGrabbedObjectOffset();
    handleInteractInput();
}

void GrabBehaviorComponent::autoReleaseIfNecessary() {
    if (!grabbedObject) {
        return;
    }

    bool shouldRelease = grabbedObject->isMarkedForDeath();

    if (!shouldRelease) {
        if (auto* objBody = grabbedObject->getComponent<BodyComponent>()) {
            shouldRelease = B2_IS_NULL(objBody->getBodyId());
        } else {
            shouldRelease = true;
        }
    }

    if (!shouldRelease && grabJoint) {
        if (grabJoint->isJointBroken()) {
            shouldRelease = true;
        }
    }

    if (shouldRelease) {
        releaseGrabbedObject();
    }
}

void GrabBehaviorComponent::maintainGrabbedObjectOffset() {
    if (!grabbedObject || !grabJoint) {
        return;
    }

    if (B2_IS_NULL(grabJoint->getJointId())) {
        return;
    }

    b2JointId jointId = grabJoint->getJointId();
    if (!b2Joint_IsValid(jointId) || b2Joint_GetType(jointId) != b2_motorJoint) {
        return;
    }

    auto [playerX, playerY, playerAngleDeg] = body->getPosition();
    float playerAngleRad = Engine::degreesToRadians(playerAngleDeg);

    BodyComponent* objBody = grabbedObject->getComponent<BodyComponent>();
    if (!objBody) {
        releaseGrabbedObject();
        return;
    }

    auto [objX, objY, objAngle] = objBody->getPosition();
    (void)objAngle;

    float dx = objX - playerX;
    float dy = objY - playerY;
    float holdDistance = std::sqrt(dx * dx + dy * dy);

    float dirX = std::sin(playerAngleRad);
    float dirY = -std::cos(playerAngleRad);

    b2Vec2 newOffset = {
        (dirX * holdDistance) * Engine::PIXELS_TO_METERS,
        (dirY * holdDistance) * Engine::PIXELS_TO_METERS
    };

    b2MotorJoint_SetLinearOffset(jointId, newOffset);
}

void GrabBehaviorComponent::handleInteractInput() {
    bool interactPressed = input->isPressed(GameAction::ACTION_INTERACT);
    if (interactPressed && !wasInteractPressed) {
        if (grabbedObject) {
            releaseGrabbedObject();
        } else {
            Object* candidate = findGrabbableObject();
            if (candidate) {
                grabObject(candidate);
            }
        }
    }
    wasInteractPressed = interactPressed;
}

Object* GrabBehaviorComponent::findGrabbableObject() {
    if (!body) {
        return nullptr;
    }

    Engine* engine = Object::getEngine();
    if (!engine) {
        return nullptr;
    }

    auto [playerX, playerY, playerAngleDeg] = body->getPosition();
    float playerAngleRad = Engine::degreesToRadians(playerAngleDeg);

    float dirX = std::sin(playerAngleRad);
    float dirY = -std::cos(playerAngleRad);

    float startOffset = 10.0f;
    float rayStartX = playerX + dirX * startOffset;
    float rayStartY = playerY + dirY * startOffset;

    b2Vec2 rayOrigin = {
        rayStartX * Engine::PIXELS_TO_METERS,
        rayStartY * Engine::PIXELS_TO_METERS
    };

    float rayDistance = grabDistance - startOffset;
    if (rayDistance <= 0.0f) {
        rayDistance = grabDistance;
    }

    b2Vec2 rayTranslation = {
        dirX * rayDistance * Engine::PIXELS_TO_METERS,
        dirY * rayDistance * Engine::PIXELS_TO_METERS
    };

    b2WorldId worldId = engine->getPhysicsWorld();
    b2QueryFilter filter = b2DefaultQueryFilter();
    b2RayResult result = b2World_CastRayClosest(worldId, rayOrigin, rayTranslation, filter);

    if (!result.hit || B2_IS_NULL(result.shapeId)) {
        return nullptr;
    }

    b2BodyId hitBodyId = b2Shape_GetBody(result.shapeId);
    void* userData = b2Body_GetUserData(hitBodyId);
    if (!userData) {
        return nullptr;
    }

    Object* hitObject = static_cast<Object*>(userData);
    if (hitObject == &parent()) {
        return nullptr;
    }

    if (!isGrabbable(hitObject)) {
        return nullptr;
    }

    return hitObject;
}

bool GrabBehaviorComponent::isGrabbable(Object* candidate) const {
    if (!candidate) {
        return false;
    }

    BodyComponent* targetBody = candidate->getComponent<BodyComponent>();
    if (!targetBody) {
        return false;
    }

    if (!passesDensityLimit(targetBody)) {
        return false;
    }

    b2BodyId bodyId = targetBody->getBodyId();
    if (B2_IS_NULL(bodyId)) {
        return false;
    }

    b2BodyType type = b2Body_GetType(bodyId);
    return type == b2_dynamicBody;
}

bool GrabBehaviorComponent::passesDensityLimit(BodyComponent* targetBody) const {
    if (maxGrabDensity < 0.0f || !targetBody) {
        return true;
    }

    b2BodyId bodyId = targetBody->getBodyId();
    if (B2_IS_NULL(bodyId)) {
        return false;
    }

    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount == 0) {
        return true;
    }

    std::vector<b2ShapeId> shapes(shapeCount);
    b2Body_GetShapes(bodyId, shapes.data(), shapeCount);

    for (const b2ShapeId& shapeId : shapes) {
        if (B2_IS_NULL(shapeId)) {
            continue;
        }

        float density = b2Shape_GetDensity(shapeId);
        if (density > maxGrabDensity) {
            return false;
        }
    }

    return true;
}

void GrabBehaviorComponent::grabObject(Object* obj) {
    if (!obj || !body) {
        return;
    }

    grabbedObject = obj;

    auto [playerX, playerY, playerAngleDeg] = body->getPosition();
    float playerAngleRad = Engine::degreesToRadians(playerAngleDeg);

    BodyComponent* objBody = obj->getComponent<BodyComponent>();
    if (!objBody) {
        grabbedObject = nullptr;
        return;
    }

    auto [objX, objY, objAngleDeg] = objBody->getPosition();
    float objAngleRad = Engine::degreesToRadians(objAngleDeg);

    float dx = objX - playerX;
    float dy = objY - playerY;
    float holdDistance = std::sqrt(dx * dx + dy * dy);

    float dirX = std::sin(playerAngleRad);
    float dirY = -std::cos(playerAngleRad);

    b2Vec2 grabOffset = {
        (dirX * holdDistance) * Engine::PIXELS_TO_METERS,
        (dirY * holdDistance) * Engine::PIXELS_TO_METERS
    };

    float angularOffsetRad = objAngleRad - playerAngleRad;
    while (angularOffsetRad > M_PI) angularOffsetRad -= 2.0f * M_PI;
    while (angularOffsetRad < -M_PI) angularOffsetRad += 2.0f * M_PI;
    float angularOffsetDeg = Engine::radiansToDegrees(angularOffsetRad);

    if (!grabJoint) {
        grabJoint = parent().addComponent<JointComponent>();
    }

    grabJoint->createMotorJoint(
        grabbedObject,
        grabOffset,
        angularOffsetDeg,
        grabForce * 3.0f,
        50.0f
    );

    grabJoint->setBreakForce(breakForce);

    if (!sound) {
        sound = parent().getComponent<SoundComponent>();
    }
    if (sound) {
        sound->playActionSound("grab");
    }
}

void GrabBehaviorComponent::releaseGrabbedObject(bool playSound) {
    if (!grabbedObject) {
        return;
    }

    if (grabJoint) {
        grabJoint->destroyJoint();
    }

    if (!sound) {
        sound = parent().getComponent<SoundComponent>();
    }
    if (sound && grabbedObject && playSound) {
        sound->playActionSound("grab_release");
    }

    grabbedObject = nullptr;
}

Object* GrabBehaviorComponent::detachGrabbedObject(bool playSound) {
    if (!grabbedObject) {
        return nullptr;
    }

    Object* objectToReturn = grabbedObject;
    releaseGrabbedObject(playSound);
    return objectToReturn;
}

static ComponentRegistrar<GrabBehaviorComponent> registrar("GrabBehaviorComponent");


