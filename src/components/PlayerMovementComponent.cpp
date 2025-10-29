#include "PlayerMovementComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include <iostream>

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
    
    // Calculate velocity in pixels per second
    // We set the velocity directly rather than modifying, since this is the primary movement source
    float velocityX = horizontal * currentSpeed;
    float velocityY = vertical * currentSpeed;
    
    // Set velocity on the body component
    // Other components can use modVelocity to add additional forces
    // BodyComponent will apply drag and handle updating position based on velocity
    body->modVelocity(velocityX, velocityY, 0.0f);
    
    // Debug output for actions
    if (input->isPressed(GameAction::ACTION_INTERACT)) {
        std::cout << "Action Interact pressed!" << std::endl;
    }
    if (input->isPressed(GameAction::ACTION_THROW)) {
        std::cout << "Action Throw pressed!" << std::endl;
    }
}

