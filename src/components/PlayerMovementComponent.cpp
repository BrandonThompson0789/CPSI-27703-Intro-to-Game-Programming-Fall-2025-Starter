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
    sprite = parent.getComponent<SpriteComponent>();

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
    sprite = parent.getComponent<SpriteComponent>();
    
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

    
    // Calculate input magnitude to determine if player is trying to move
    float inputMagnitude = std::sqrt(horizontal * horizontal + vertical * vertical);
    
    // Normalize the movement vector to prevent faster diagonal movement
    float normalizedHorizontal = horizontal;
    float normalizedVertical = vertical;
    if (inputMagnitude > 0.0f) {
        normalizedHorizontal /= inputMagnitude;
        normalizedVertical /= inputMagnitude;
    }
    
    // Apply movement using Box2D forces
    // modVelocity adds to current velocity - Box2D will handle damping
    float velocityX = normalizedHorizontal * currentSpeed * 0.1f; // Scale down for Box2D force application
    float velocityY = normalizedVertical * currentSpeed * 0.1f;
    body->modVelocity(velocityX, velocityY, 0.0f);
    
    // Update sprite based on actual velocity (for animation)
    auto [actualVelX, actualVelY, actualVelAngle] = body->getVelocity();
    float moveMagnitude = std::sqrt(actualVelX * actualVelX + actualVelY * actualVelY);
    
    if (sprite) {
        if (moveMagnitude > 1.0f) {
            sprite->setCurrentSprite("player_walking");
        } else {
            sprite->setCurrentSprite("player_standing");
        }
    }

    // Rotate toward INPUT direction (where player wants to go, not where they're going)
    if (inputMagnitude > 0.1f) { // Only rotate if player is giving input
        // Calculate target angle from INPUT direction (in radians)
        // atan2(y, x) gives 0 radians pointing East (right)
        // We subtract π/2 to rotate the coordinate system so 0 radians points North (up)
        // Then add π to flip it 180 degrees so the sprite faces forward
        float targetAngle = std::atan2(vertical, horizontal) - M_PI / 2.0f + M_PI;
        
        // Get current angle from body
        auto [posX, posY, currentAngle] = body->getPosition();
        
        // Calculate the shortest angle difference to rotate toward target
        float angleDiff = targetAngle - currentAngle;
        
        // Normalize angle difference to [-PI, PI] range
        while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
        while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
        
        // Set angular velocity to smoothly rotate toward target
        // The rotation speed scales with the angle difference for smooth turning
        float rotationSpeed = 0.5f; // Lower = slower, more gradual turning
        float targetAngularVelocity = angleDiff * rotationSpeed;
        
        // Clamp angular velocity to prevent overshooting
        float maxAngularVelocity = 0.5f; // Maximum rotation speed in radians/second (lower = slower max turn)
        if (targetAngularVelocity > maxAngularVelocity) targetAngularVelocity = maxAngularVelocity;
        if (targetAngularVelocity < -maxAngularVelocity) targetAngularVelocity = -maxAngularVelocity;
        
        // Apply angular velocity
        body->modVelocity(0.0f, 0.0f, targetAngularVelocity);
    } else {
        // When no input, let angular damping naturally slow rotation
        // (Box2D's angularDamping will handle this automatically)
    }
    
    // Debug output for actions
    if (input->isPressed(GameAction::ACTION_INTERACT)) {
        std::cout << "Action Interact pressed!" << std::endl;
    }
    if (input->isPressed(GameAction::ACTION_THROW)) {
        std::cout << "Action Throw pressed!" << std::endl;
    }
}

