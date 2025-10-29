#include "PlayerMovementComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include <iostream>
#include <cmath>

PlayerMovementComponent::PlayerMovementComponent(Object& parent, float moveSpeed)
    : Component(parent)
    , moveSpeed(moveSpeed)
{
    // Get required components
    input = parent.getComponent<InputComponent>();
    body = parent.getComponent<BodyComponent>();
    
    if (!input) {
        std::cerr << "Warning: PlayerMovementComponent requires InputComponent!" << std::endl;
    }
    if (!body) {
        std::cerr << "Warning: PlayerMovementComponent requires BodyComponent!" << std::endl;
    }
}

PlayerMovementComponent::PlayerMovementComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , moveSpeed(200.0f)
{
    if (data.contains("moveSpeed")) moveSpeed = data["moveSpeed"].get<float>();
    
    // Get required components (must exist on parent)
    input = parent.getComponent<InputComponent>();
    body = parent.getComponent<BodyComponent>();
    
    if (!input) {
        std::cerr << "Warning: PlayerMovementComponent requires InputComponent!" << std::endl;
    }
    if (!body) {
        std::cerr << "Warning: PlayerMovementComponent requires BodyComponent!" << std::endl;
    }
}

nlohmann::json PlayerMovementComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["moveSpeed"] = moveSpeed;
    return j;
}

// Register this component type with the library
static ComponentRegistrar<PlayerMovementComponent> registrar("PlayerMovementComponent");

void PlayerMovementComponent::update(float deltaTime) {
    if (!input || !body) return;
    
    // Check if input source is active
    if (!input->isActive()) {
        std::cerr << "Warning: Input source not active!" << std::endl;
        return;
    }
    
    // Get movement input (0.0 to 1.0 for each direction)
    float moveUp = input->getMoveUp();
    float moveDown = input->getMoveDown();
    float moveLeft = input->getMoveLeft();
    float moveRight = input->getMoveRight();
    
    // Calculate net movement (-1.0 to 1.0 for each axis)
    float horizontal = moveRight - moveLeft;
    float vertical = moveDown - moveUp;
    
    // Get walk modifier (0.0 to 1.0)
    float walkModifier = input->getActionWalk();
    
    // Calculate speed with walk modifier
    // When walkModifier is 0.0: full speed
    // When walkModifier is 1.0: half speed
    float currentSpeed = moveSpeed * (1.0f - walkModifier * 0.5f);
    
    // Apply velocity from input to the body component
    float velocityX = horizontal * currentSpeed;
    float velocityY = vertical * currentSpeed;
    body->modVelocity(velocityX, velocityY, 0.0f);
    
    // After applying velocity, get the actual current velocity from the body
    auto [actualVelX, actualVelY, actualVelAngle] = body->getVelocity();
    
    // Calculate rotation toward actual movement direction if moving
    float moveMagnitude = std::sqrt(actualVelX * actualVelX + actualVelY * actualVelY);
    if (moveMagnitude > 0.01f) { // Only rotate if moving meaningfully
        // Calculate target angle from actual velocity direction (in radians)
        // Note: atan2(y, x) gives 0 radians pointing East (right)
        // We subtract π/2 to rotate the coordinate system so 0 radians points North (up)
        // Then add π to flip it 180 degrees so it faces forward
        float targetAngle = std::atan2(actualVelY, actualVelX) - M_PI / 2.0f + M_PI;
        
        // Get current angle from body
        auto [posX, posY, currentAngle] = body->getPosition();
        
        // Calculate the shortest angle difference to rotate toward target
        float angleDiff = targetAngle - currentAngle;
        
        // Normalize angle difference to [-PI, PI]
        while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
        while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
        
        // Set angular velocity to rotate toward target (radians per second)
        // The rotation speed scales with the angle difference
        float rotationSpeed = 8.0f; // Base rotation speed in radians per second
        float targetAngularVelocity = angleDiff * rotationSpeed;
        
        // Clamp angular velocity to prevent overshooting
        float maxAngularVelocity = rotationSpeed * 2.0f;
        if (targetAngularVelocity > maxAngularVelocity) targetAngularVelocity = maxAngularVelocity;
        if (targetAngularVelocity < -maxAngularVelocity) targetAngularVelocity = -maxAngularVelocity;
        
        // Apply angular velocity
        body->modVelocity(0.0f, 0.0f, targetAngularVelocity);
    } else {
        // When not moving, reduce angular velocity to zero
        if (std::abs(actualVelAngle) > 0.01f) {
            // Apply damping to stop rotation
            body->modVelocity(0.0f, 0.0f, -actualVelAngle * 0.5f);
        }
    }
    
    // Debug output for actions
    if (input->isPressed(GameAction::ACTION_INTERACT)) {
        std::cout << "Action Interact pressed!" << std::endl;
    }
    if (input->isPressed(GameAction::ACTION_THROW)) {
        std::cout << "Action Throw pressed!" << std::endl;
    }
}

