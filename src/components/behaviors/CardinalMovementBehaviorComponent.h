#pragma once

#include "../Component.h"
#include "../InputComponent.h"
#include "../BodyComponent.h"
#include <nlohmann/json.hpp>

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
    void resolveDependencies();
    bool acquireInputDirection(float& outHorizontal, float& outVertical);
    void updateMovement(float desiredHorizontal, float desiredVertical, float deltaTime);
    void updateRotation(float desiredHorizontal, float desiredVertical);

    InputComponent* input;
    BodyComponent* body;

    float moveSpeed;
    float rotationResponsiveness;
    float maxAngularVelocity;
    float rotationStopThresholdDegrees;
};


