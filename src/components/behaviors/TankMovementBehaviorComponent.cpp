#include "TankMovementBehaviorComponent.h"
#include "PathfindingBehaviorComponent.h"
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
    , pathInput(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(moveSpeed)
    , moveSoundRateScale(0.02f)
    , moveSoundTimer(0.0f)
    , rotationResponsiveness(0.6f)
    , maxAngularVelocity(0.6f)
    , rotationStopThresholdDegrees(5.0f)
    , pathfindingCardinalCostScale(1.0f)
    , pathfindingDiagonalCostScale(1.15f)
    , pathfindingTurnPenalty(0.35f)
    , pathfindingCourtesyTurnThreshold(2) {
    resolveDependencies();
}

TankMovementBehaviorComponent::TankMovementBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , input(nullptr)
    , pathInput(nullptr)
    , body(nullptr)
    , sound(nullptr)
    , moveSpeed(data.value("moveSpeed", 200.0f))
    , moveSoundRateScale(data.value("moveSoundRateScale", 0.02f))
    , moveSoundTimer(0.0f)
    , rotationResponsiveness(data.value("rotationResponsiveness", 0.6f))
    , maxAngularVelocity(data.value("maxAngularVelocity", 0.6f))
    , rotationStopThresholdDegrees(data.value("rotationStopThresholdDegrees", 5.0f))
    , pathfindingCardinalCostScale(data.value("pathfindingCardinalCostScale", 1.0f))
    , pathfindingDiagonalCostScale(data.value("pathfindingDiagonalCostScale", 1.15f))
    , pathfindingTurnPenalty(data.value("pathfindingTurnPenalty", 0.35f))
    , pathfindingCourtesyTurnThreshold(data.value("pathfindingCourtesyTurnThreshold", 2)) {
    resolveDependencies();
}

void TankMovementBehaviorComponent::resolveDependencies() {
    input = parent().getComponent<InputComponent>();
    if (!pathInput) {
        pathInput = parent().getComponent<PathfindingBehaviorComponent>();
    }
    body = parent().getComponent<BodyComponent>();
    sound = parent().getComponent<SoundComponent>();

    if (!input && !pathInput) {
        std::cerr << "Warning: TankMovementBehaviorComponent requires an InputComponent or PathfindingBehaviorComponent!\n";
    }
    if (!body) {
        std::cerr << "Warning: TankMovementBehaviorComponent requires BodyComponent!\n";
    }
    applyPathfindingBias();
}

nlohmann::json TankMovementBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["moveSpeed"] = moveSpeed;
    j["moveSoundRateScale"] = moveSoundRateScale;
    j["rotationResponsiveness"] = rotationResponsiveness;
    j["maxAngularVelocity"] = maxAngularVelocity;
    j["rotationStopThresholdDegrees"] = rotationStopThresholdDegrees;
    j["pathfindingCardinalCostScale"] = pathfindingCardinalCostScale;
    j["pathfindingDiagonalCostScale"] = pathfindingDiagonalCostScale;
    j["pathfindingTurnPenalty"] = pathfindingTurnPenalty;
    j["pathfindingCourtesyTurnThreshold"] = pathfindingCourtesyTurnThreshold;
    return j;
}

void TankMovementBehaviorComponent::update(float deltaTime) {
    if (!body) {
        return;
    }

    MovementInputSample inputSample = gatherMovementInput();

    if (!inputSample.active) {
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
    bool hasDirection = acquireInputDirection(desiredHorizontal, desiredVertical, inputSample);
    hasDirection = applyPathfindingCourtesy(inputSample, desiredHorizontal, desiredVertical, hasDirection);

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

bool TankMovementBehaviorComponent::acquireInputDirection(float& outHorizontal, float& outVertical, const MovementInputSample& inputSample) {
    float horizontal = inputSample.moveRight - inputSample.moveLeft;
    float vertical = inputSample.moveDown - inputSample.moveUp;

    auto [quantizedHorizontal, quantizedVertical] = quantizeToCardinal(horizontal, vertical);
    outHorizontal = quantizedHorizontal;
    outVertical = quantizedVertical;

    return !(quantizedHorizontal == 0.0f && quantizedVertical == 0.0f);
}

TankMovementBehaviorComponent::MovementInputSample TankMovementBehaviorComponent::gatherMovementInput() {
    MovementInputSample sample;

    if (!pathInput) {
        pathInput = parent().getComponent<PathfindingBehaviorComponent>();
        applyPathfindingBias();
    }

    if (pathInput && pathInput->isActive()) {
        sample.moveUp = pathInput->getMoveUp();
        sample.moveDown = pathInput->getMoveDown();
        sample.moveLeft = pathInput->getMoveLeft();
        sample.moveRight = pathInput->getMoveRight();
        sample.walk = pathInput->getActionWalk();
        sample.active = true;
        sample.fromPathfinder = true;
        return sample;
    }

    if (input && input->isActive()) {
        sample.moveUp = input->getMoveUp();
        sample.moveDown = input->getMoveDown();
        sample.moveLeft = input->getMoveLeft();
        sample.moveRight = input->getMoveRight();
        sample.walk = input->getActionWalk();
        sample.active = true;
        return sample;
    }

    return sample;
}

void TankMovementBehaviorComponent::applyPathfindingBias() {
    if (pathInput) {
        pathInput->setDirectionCostBias(pathfindingCardinalCostScale, pathfindingDiagonalCostScale);
        pathInput->setTurnCostPenalty(pathfindingTurnPenalty);
    }
}

bool TankMovementBehaviorComponent::applyPathfindingCourtesy(const MovementInputSample& sample,
                                                             float& desiredHorizontal,
                                                             float& desiredVertical,
                                                             bool hasDirection) {
    auto resetHistory = [&]() {
        pathfindingPendingDirectionChanges = 0;
        pathfindingHasLastDirection = false;
        pathfindingLastDirection = {0.0f, 0.0f};
    };

    if (!sample.fromPathfinder || pathfindingCourtesyTurnThreshold <= 0) {
        resetHistory();
        return hasDirection;
    }

    if (!hasDirection) {
        resetHistory();
        return false;
    }

    auto sameDirection = [&](float h, float v) {
        return std::abs(h - pathfindingLastDirection.first) < 0.0001f &&
               std::abs(v - pathfindingLastDirection.second) < 0.0001f;
    };

    if (!pathfindingHasLastDirection) {
        pathfindingLastDirection = {desiredHorizontal, desiredVertical};
        pathfindingHasLastDirection = true;
        pathfindingPendingDirectionChanges = 0;
        return true;
    }

    if (sameDirection(desiredHorizontal, desiredVertical)) {
        pathfindingPendingDirectionChanges = 0;
        return true;
    }

    pathfindingPendingDirectionChanges++;
    if (pathfindingPendingDirectionChanges < pathfindingCourtesyTurnThreshold) {
        desiredHorizontal = pathfindingLastDirection.first;
        desiredVertical = pathfindingLastDirection.second;
        return (desiredHorizontal != 0.0f || desiredVertical != 0.0f);
    }

    // After exceeding threshold, aim directly at nearest waypoint/destination
    bool replaced = false;
    if (pathInput && body) {
        auto targetPoint = pathInput->getActiveWaypoint();
        if (!targetPoint.has_value()) {
            targetPoint = pathInput->getDestinationPoint();
        }
        if (targetPoint.has_value()) {
            auto [posX, posY, _] = body->getPosition();
            float dx = targetPoint->x - posX;
            float dy = targetPoint->y - posY;
            float magnitude = std::sqrt(dx * dx + dy * dy);
            if (magnitude > 0.001f) {
                desiredHorizontal = dx / magnitude;
                desiredVertical = dy / magnitude;
                replaced = true;
            }
        }
    }

    if (!replaced) {
        // Fall back to requested direction
        replaced = true;
    }

    pathfindingLastDirection = {desiredHorizontal, desiredVertical};
    pathfindingPendingDirectionChanges = 0;
    pathfindingHasLastDirection = true;
    return replaced && (desiredHorizontal != 0.0f || desiredVertical != 0.0f);
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


