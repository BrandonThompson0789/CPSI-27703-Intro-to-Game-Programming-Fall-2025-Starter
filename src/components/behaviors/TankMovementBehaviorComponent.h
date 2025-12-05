#pragma once

#include "../Component.h"
#include "../InputComponent.h"
#include "../BodyComponent.h"
#include <nlohmann/json.hpp>

class SoundComponent;
class PathfindingBehaviorComponent;

/**
 * Movement behaviour that restricts motion to the eight cardinal/intercardinal
 * directions. When switching directions, the player stops translating until
 * rotation aligns with the new facing.
 */
class TankMovementBehaviorComponent : public Component {
public:
    TankMovementBehaviorComponent(Object& parent, float moveSpeed = 200.0f);
    TankMovementBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~TankMovementBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "TankMovementBehaviorComponent"; }

    void setMoveSpeed(float speed) { moveSpeed = speed; }
    float getMoveSpeed() const { return moveSpeed; }

    void setRotationResponsiveness(float responsiveness) { rotationResponsiveness = responsiveness; }
    float getRotationResponsiveness() const { return rotationResponsiveness; }

    void setMaxAngularVelocity(float maxVelocity) { maxAngularVelocity = maxVelocity; }
    float getMaxAngularVelocity() const { return maxAngularVelocity; }

    void setRotationStopThreshold(float degrees) { rotationStopThresholdDegrees = degrees; }
    float getRotationStopThreshold() const { return rotationStopThresholdDegrees; }

private:
    struct MovementInputSample;
    void resolveDependencies();
    bool acquireInputDirection(float& outHorizontal, float& outVertical, const MovementInputSample& inputSample);
    struct MovementInputSample {
        float moveUp = 0.0f;
        float moveDown = 0.0f;
        float moveLeft = 0.0f;
        float moveRight = 0.0f;
        float walk = 0.0f;
        bool active = false;
        bool fromPathfinder = false;
    };
    MovementInputSample gatherMovementInput();
    void updateMovement(float desiredHorizontal, float desiredVertical, float deltaTime);
    void updateRotation(float desiredHorizontal, float desiredVertical);
    void applyPathfindingBias();
    bool applyPathfindingCourtesy(const MovementInputSample& sample, float& desiredHorizontal, float& desiredVertical, bool hasDirection);

    InputComponent* input;
    PathfindingBehaviorComponent* pathInput;
    BodyComponent* body;
    SoundComponent* sound;

    float moveSpeed;
    float moveSoundRateScale;
    float moveSoundTimer;
    float rotationResponsiveness;
    float maxAngularVelocity;
    float rotationStopThresholdDegrees;
    float pathfindingCardinalCostScale;
    float pathfindingDiagonalCostScale;
    float pathfindingTurnPenalty;
    int pathfindingCourtesyTurnThreshold;
    int pathfindingPendingDirectionChanges = 0;
    std::pair<float, float> pathfindingLastDirection{0.0f, 0.0f};
    bool pathfindingHasLastDirection = false;
    bool wasMoving = false;
};


