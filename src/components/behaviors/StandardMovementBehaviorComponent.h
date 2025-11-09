#pragma once

#include "../Component.h"
#include "../InputComponent.h"
#include "../BodyComponent.h"
#include <nlohmann/json.hpp>

/**
 * Handles standard player movement behaviour: reading movement input,
 * applying velocity to the physics body, and updating the sprite facing.
 */
class StandardMovementBehaviorComponent : public Component {
public:
    StandardMovementBehaviorComponent(Object& parent, float moveSpeed = 200.0f);
    StandardMovementBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~StandardMovementBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "StandardMovementBehaviorComponent"; }

    void setMoveSpeed(float speed) { moveSpeed = speed; }
    float getMoveSpeed() const { return moveSpeed; }

    void setRotationResponsiveness(float responsiveness) { rotationResponsiveness = responsiveness; }
    float getRotationResponsiveness() const { return rotationResponsiveness; }

    void setMaxAngularVelocity(float maxVelocity) { maxAngularVelocity = maxVelocity; }
    float getMaxAngularVelocity() const { return maxAngularVelocity; }

private:
    void resolveDependencies();
    void updateRotation(float inputHorizontal, float inputVertical);

    InputComponent* input;
    BodyComponent* body;

    float moveSpeed;
    float walkSlowdownFactor;
    float rotationResponsiveness;
    float maxAngularVelocity;
};


