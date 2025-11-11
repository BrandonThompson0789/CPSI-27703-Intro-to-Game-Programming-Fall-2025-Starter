#include "ThrowBehaviorComponent.h"
#include "GrabBehaviorComponent.h"
#include "../ComponentLibrary.h"
#include "../SoundComponent.h"
#include "../../Engine.h"
#include "../../Object.h"
#include <cmath>
#include <iostream>

ThrowBehaviorComponent::ThrowBehaviorComponent(Object& parent)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , grabBehavior(nullptr)
    , soundComponent(nullptr)
    , maxThrowCharge(1.0f)
    , throwForceMultiplier(1000.0f)
    , minThrowForceRatio(0.3f)
    , throwChargeTime(0.0f)
    , isChargingThrow(false)
    , wasThrowPressed(false) {
    resolveDependencies();
}

ThrowBehaviorComponent::ThrowBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , grabBehavior(nullptr)
    , soundComponent(nullptr)
    , maxThrowCharge(data.value("maxThrowCharge", 1.0f))
    , throwForceMultiplier(data.value("throwForceMultiplier", 1000.0f))
    , minThrowForceRatio(data.value("minThrowForceRatio", 0.3f))
    , throwChargeTime(0.0f)
    , isChargingThrow(false)
    , wasThrowPressed(false) {
    resolveDependencies();
}

void ThrowBehaviorComponent::resolveDependencies() {
    input = parent().getComponent<InputComponent>();
    body = parent().getComponent<BodyComponent>();
    grabBehavior = parent().getComponent<GrabBehaviorComponent>();
    soundComponent = parent().getComponent<SoundComponent>();

    if (!input) {
        std::cerr << "Warning: ThrowBehaviorComponent requires InputComponent!\n";
    }
    if (!body) {
        std::cerr << "Warning: ThrowBehaviorComponent requires BodyComponent!\n";
    }
    if (!grabBehavior) {
        std::cerr << "Warning: ThrowBehaviorComponent requires GrabBehaviorComponent!\n";
    }
}

nlohmann::json ThrowBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["maxThrowCharge"] = maxThrowCharge;
    j["throwForceMultiplier"] = throwForceMultiplier;
    j["minThrowForceRatio"] = minThrowForceRatio;
    return j;
}

void ThrowBehaviorComponent::update(float deltaTime) {
    if (!input || !body) {
        return;
    }

    if (!grabBehavior) {
        // Dependency may have been added after construction; attempt to resolve lazily.
        grabBehavior = parent().getComponent<GrabBehaviorComponent>();
        if (!soundComponent) {
            soundComponent = parent().getComponent<SoundComponent>();
        }
        if (!grabBehavior) {
            return;
        }
    }

    if (!grabBehavior->hasGrabbedObject()) {
        throwChargeTime = 0.0f;
        isChargingThrow = false;
        wasThrowPressed = false;
        return;
    }

    updateChargeState(deltaTime);
}

void ThrowBehaviorComponent::updateChargeState(float deltaTime) {
    bool throwPressed = input->isPressed(GameAction::ACTION_THROW);

    if (throwPressed && !wasThrowPressed) {
        isChargingThrow = true;
        throwChargeTime = 0.0f;
    }

    if (throwPressed && isChargingThrow) {
        throwChargeTime += deltaTime;
        if (throwChargeTime > maxThrowCharge) {
            throwChargeTime = maxThrowCharge;
        }
    }

    if (!throwPressed && wasThrowPressed && isChargingThrow) {
        float chargeRatio = (maxThrowCharge > 0.0f) ? (throwChargeTime / maxThrowCharge) : 1.0f;
        if (chargeRatio < minThrowForceRatio) {
            chargeRatio = minThrowForceRatio;
        }
        executeThrow(chargeRatio);
        isChargingThrow = false;
        throwChargeTime = 0.0f;
    }

    wasThrowPressed = throwPressed;
}

void ThrowBehaviorComponent::executeThrow(float chargeRatio) {
    if (!grabBehavior || !body) {
        return;
    }

    Object* objectToThrow = grabBehavior->detachGrabbedObject(false);
    if (!objectToThrow) {
        return;
    }

    auto [playerX, playerY, playerAngleDeg] = body->getPosition();
    (void)playerX;
    (void)playerY;

    float playerAngleRad = Engine::degreesToRadians(playerAngleDeg);
    float dirX = std::sin(playerAngleRad);
    float dirY = -std::cos(playerAngleRad);

    float throwForce = throwForceMultiplier * chargeRatio;

    BodyComponent* targetBody = objectToThrow->getComponent<BodyComponent>();
    if (!targetBody) {
        return;
    }

    auto [currentVelX, currentVelY, currentVelAngle] = targetBody->getVelocity();
    (void)currentVelAngle;

    float throwVelX = dirX * throwForce;
    float throwVelY = dirY * throwForce;

    targetBody->setVelocity(
        currentVelX + throwVelX,
        currentVelY + throwVelY,
        0.0f
    );

    if (!soundComponent) {
        soundComponent = parent().getComponent<SoundComponent>();
    }
    if (soundComponent) {
        soundComponent->playActionSound("throw");
    }
}

static ComponentRegistrar<ThrowBehaviorComponent> registrar("ThrowBehaviorComponent");


