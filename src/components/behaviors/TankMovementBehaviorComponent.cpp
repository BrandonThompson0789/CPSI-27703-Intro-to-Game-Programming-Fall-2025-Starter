#include "TankMovementBehaviorComponent.h"
#include "../ComponentLibrary.h"
#include "../SoundComponent.h"
#include "../../Engine.h"
#include "../../Object.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = PI * 2.0f;
constexpr float CARDINAL_STEP = PI / 4.0f;

float normalizeAngle(float angle) {
    while (angle > PI) angle -= TWO_PI;
    while (angle < -PI) angle += TWO_PI;
    return angle;
}

std::pair<float, float> quantizeToCardinal(float horizontal, float vertical) {
    float magnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    if (magnitude <= 0.0f) {
        return {0.0f, 0.0f};
    }

    float angle = std::atan2(vertical, horizontal);
    float quantizedAngle = std::round(angle / CARDINAL_STEP) * CARDINAL_STEP;

    float quantizedHorizontal = std::cos(quantizedAngle);
    float quantizedVertical = std::sin(quantizedAngle);

    // Avoid tiny floating-point values
    if (std::abs(quantizedHorizontal) < 0.0001f) quantizedHorizontal = 0.0f;
    if (std::abs(quantizedVertical) < 0.0001f) quantizedVertical = 0.0f;

    return {quantizedHorizontal, quantizedVertical};
}
} // namespace

TankMovementBehaviorComponent::TankMovementBehaviorComponent(Object& parent, float moveSpeed)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(moveSpeed)
    , moveSoundRateScale(0.02f)
    , moveSoundTimer(0.0f)
    , rotationResponsiveness(0.6f)
    , maxAngularVelocity(0.6f)
    , rotationStopThresholdDegrees(5.0f) {
    resolveDependencies();
}

TankMovementBehaviorComponent::TankMovementBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , input(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(data.value("moveSpeed", 200.0f))
    , moveSoundRateScale(data.value("moveSoundRateScale", 0.02f))
    , moveSoundTimer(0.0f)
    , rotationResponsiveness(data.value("rotationResponsiveness", 0.6f))
    , maxAngularVelocity(data.value("maxAngularVelocity", 0.6f))
    , rotationStopThresholdDegrees(data.value("rotationStopThresholdDegrees", 5.0f)) {
    resolveDependencies();
}

void TankMovementBehaviorComponent::resolveDependencies() {
    input = parent().getComponent<InputComponent>();
    body = parent().getComponent<BodyComponent>();
    sound = parent().getComponent<SoundComponent>();

    if (!input) {
        std::cerr << "Warning: TankMovementBehaviorComponent requires InputComponent!\n";
    }
    if (!body) {
        std::cerr << "Warning: TankMovementBehaviorComponent requires BodyComponent!\n";
    }
}

nlohmann::json TankMovementBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["moveSpeed"] = moveSpeed;
    j["moveSoundRateScale"] = moveSoundRateScale;
    j["rotationResponsiveness"] = rotationResponsiveness;
    j["maxAngularVelocity"] = maxAngularVelocity;
    j["rotationStopThresholdDegrees"] = rotationStopThresholdDegrees;
    return j;
}

void TankMovementBehaviorComponent::update(float deltaTime) {
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

    float desiredHorizontal = 0.0f;
    float desiredVertical = 0.0f;
    bool hasDirection = acquireInputDirection(desiredHorizontal, desiredVertical);

    bool isMoving = hasDirection;
    if (!sound) {
        sound = parent().getComponent<SoundComponent>();
    }
    if (sound) {
        if (isMoving) {
            moveSoundTimer -= deltaTime;
            if (moveSoundTimer <= 0.0f) {
                float rateScale = std::clamp(moveSoundRateScale, 0.001f, 10.0f);
                float speedFactor = std::max(moveSpeed, 1.0f);
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

    if (hasDirection) {
        updateRotation(desiredHorizontal, desiredVertical);
    }

    updateMovement(hasDirection ? desiredHorizontal : 0.0f,
                   hasDirection ? desiredVertical : 0.0f,
                   deltaTime);
}

bool TankMovementBehaviorComponent::acquireInputDirection(float& outHorizontal, float& outVertical) {
    float moveUp = input->getMoveUp();
    float moveDown = input->getMoveDown();
    float moveLeft = input->getMoveLeft();
    float moveRight = input->getMoveRight();

    float horizontal = moveRight - moveLeft;
    float vertical = moveDown - moveUp;

    auto [quantizedHorizontal, quantizedVertical] = quantizeToCardinal(horizontal, vertical);
    outHorizontal = quantizedHorizontal;
    outVertical = quantizedVertical;

    return !(quantizedHorizontal == 0.0f && quantizedVertical == 0.0f);
}

void TankMovementBehaviorComponent::updateMovement(float desiredHorizontal, float desiredVertical, float deltaTime) {
    (void)deltaTime;

    float movementMagnitude = std::sqrt(desiredHorizontal * desiredHorizontal + desiredVertical * desiredVertical);

    if (movementMagnitude > 0.0f) {
        auto [posX, posY, currentAngleDeg] = body->getPosition();
        (void)posX;
        (void)posY;

        float targetAngleRad = std::atan2(desiredVertical, desiredHorizontal) - PI / 2.0f + PI;
        float currentAngleRad = Engine::degreesToRadians(currentAngleDeg);
        float angleDiff = normalizeAngle(targetAngleRad - currentAngleRad);
        float angleDiffDegrees = std::abs(Engine::radiansToDegrees(angleDiff));

        if (angleDiffDegrees <= rotationStopThresholdDegrees) {
            float velocityX = desiredHorizontal * moveSpeed * 0.1f;
            float velocityY = desiredVertical * moveSpeed * 0.1f;
            body->modVelocity(velocityX, velocityY, 0.0f);
        } else {
            auto [currentVelX, currentVelY, currentVelAngle] = body->getVelocity();
            body->setVelocity(0.0f, 0.0f, currentVelAngle);
        }
    }

}

void TankMovementBehaviorComponent::updateRotation(float desiredHorizontal, float desiredVertical) {
    if (!body) {
        return;
    }

    float inputMagnitude = std::sqrt(desiredHorizontal * desiredHorizontal + desiredVertical * desiredVertical);
    if (inputMagnitude <= 0.0f) {
        return;
    }

    float targetAngleRad = std::atan2(desiredVertical, desiredHorizontal) - PI / 2.0f + PI;

    auto [posX, posY, currentAngleDeg] = body->getPosition();
    (void)posX;
    (void)posY;
    float currentAngleRad = Engine::degreesToRadians(currentAngleDeg);

    float angleDiff = normalizeAngle(targetAngleRad - currentAngleRad);
    float targetAngularVelocityRad = angleDiff * rotationResponsiveness;

    if (targetAngularVelocityRad > maxAngularVelocity) targetAngularVelocityRad = maxAngularVelocity;
    if (targetAngularVelocityRad < -maxAngularVelocity) targetAngularVelocityRad = -maxAngularVelocity;

    body->modVelocity(0.0f, 0.0f, Engine::radiansToDegrees(targetAngularVelocityRad));
}

static ComponentRegistrar<TankMovementBehaviorComponent> registrar("TankMovementBehaviorComponent");


