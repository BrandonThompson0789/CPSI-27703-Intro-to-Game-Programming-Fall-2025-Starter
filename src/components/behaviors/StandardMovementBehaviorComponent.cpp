#include "StandardMovementBehaviorComponent.h"
#include "../ComponentLibrary.h"
#include "../SoundComponent.h"
#include "../../Engine.h"
#include "../../Object.h"
#include <algorithm>
#include <cmath>
#include <iostream>

StandardMovementBehaviorComponent::StandardMovementBehaviorComponent(Object& parent, float moveSpeed)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(moveSpeed)
    , moveSoundRateScale(0.02f)
    , moveSoundTimer(0.0f)
    , walkSlowdownFactor(0.5f)
    , rotationResponsiveness(0.5f)
    , maxAngularVelocity(0.5f) {
    resolveDependencies();
}

StandardMovementBehaviorComponent::StandardMovementBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(data.value("moveSpeed", 200.0f))
    , moveSoundRateScale(data.value("moveSoundRateScale", 0.02f))
    , moveSoundTimer(0.0f)
    , walkSlowdownFactor(data.value("walkSlowdownFactor", 0.5f))
    , rotationResponsiveness(data.value("rotationResponsiveness", 0.5f))
    , maxAngularVelocity(data.value("maxAngularVelocity", 0.5f)) {

    // Support legacy configuration name
    if (data.contains("rotationSpeed")) {
        float degreesPerSecond = data["rotationSpeed"].get<float>();
        maxAngularVelocity = Engine::degreesToRadians(degreesPerSecond);
    }

    resolveDependencies();
}

void StandardMovementBehaviorComponent::resolveDependencies() {
    input = parent().getComponent<InputComponent>();
    body = parent().getComponent<BodyComponent>();
    sound = parent().getComponent<SoundComponent>();

    if (!input) {
        std::cerr << "Warning: StandardMovementBehaviorComponent requires InputComponent!\n";
    }
    if (!body) {
        std::cerr << "Warning: StandardMovementBehaviorComponent requires BodyComponent!\n";
    }
}

nlohmann::json StandardMovementBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["moveSpeed"] = moveSpeed;
    j["moveSoundRateScale"] = moveSoundRateScale;
    j["walkSlowdownFactor"] = walkSlowdownFactor;
    j["rotationResponsiveness"] = rotationResponsiveness;
    j["maxAngularVelocity"] = maxAngularVelocity;
    return j;
}

void StandardMovementBehaviorComponent::update(float deltaTime) {
    if (!input || !body) {
        return;
    }

    if (!input->isActive()) {
        if (wasMoving) {
            if (!sound) {
                sound = parent().getComponent<SoundComponent>();
            }
            if (sound) {
                sound->playActionSound("move_stop");
            }
            wasMoving = false;
        }
        moveSoundTimer = 0.0f;
        return;
    }

    float moveUp = input->getMoveUp();
    float moveDown = input->getMoveDown();
    float moveLeft = input->getMoveLeft();
    float moveRight = input->getMoveRight();

    float horizontal = moveRight - moveLeft;
    float vertical = moveDown - moveUp;

    float walkModifier = input->getActionWalk();
    float currentSpeed = moveSpeed * (1.0f - walkModifier * walkSlowdownFactor);

    float inputMagnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    float normalizedHorizontal = horizontal;
    float normalizedVertical = vertical;
    if (inputMagnitude > 0.0f) {
        normalizedHorizontal /= inputMagnitude;
        normalizedVertical /= inputMagnitude;
    }

    auto [velX, velY, velAngle] = body->getVelocity();
    float speed = std::sqrt(velX * velX + velY * velY);

    float velocityX = normalizedHorizontal * currentSpeed * 0.1f;
    float velocityY = normalizedVertical * currentSpeed * 0.1f;
    body->modVelocity(velocityX, velocityY, 0.0f);
    bool isMoving = speed > 1.0f;
    if (!sound) {
        sound = parent().getComponent<SoundComponent>();
    }
    if (sound) {
        if (isMoving) {
            moveSoundTimer -= deltaTime;
            if (moveSoundTimer <= 0.0f) {
                float rateScale = std::clamp(moveSoundRateScale, 0.001f, 10.0f);
                float speedFactor = std::max(speed, 1.0f);
                float interval = 1.0f / (speedFactor * rateScale);
                interval = std::clamp(interval, 0.1f, 1.0f);
                sound->playActionSound("move");
                moveSoundTimer = interval;
            }
        } else if (wasMoving) {
            moveSoundTimer = 0.0f;
            sound->playActionSound("move_stop");
        }
    }
    wasMoving = isMoving;

    updateRotation(horizontal, vertical);
}

void StandardMovementBehaviorComponent::updateRotation(float inputHorizontal, float inputVertical) {
    if (!body) {
        return;
    }

    float inputMagnitude = std::sqrt(inputHorizontal * inputHorizontal + inputVertical * inputVertical);
    if (inputMagnitude <= 0.1f) {
        return;
    }

    float targetAngleRad = std::atan2(inputVertical, inputHorizontal) - M_PI / 2.0f + M_PI;

    auto [posX, posY, currentAngleDeg] = body->getPosition();
    (void)posX;
    (void)posY;
    float currentAngleRad = Engine::degreesToRadians(currentAngleDeg);

    float angleDiff = targetAngleRad - currentAngleRad;
    while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
    while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;

    float targetAngularVelocityRad = angleDiff * rotationResponsiveness;

    if (targetAngularVelocityRad > maxAngularVelocity) targetAngularVelocityRad = maxAngularVelocity;
    if (targetAngularVelocityRad < -maxAngularVelocity) targetAngularVelocityRad = -maxAngularVelocity;

    body->modVelocity(0.0f, 0.0f, Engine::radiansToDegrees(targetAngularVelocityRad));
}

static ComponentRegistrar<StandardMovementBehaviorComponent> registrar("StandardMovementBehaviorComponent");


