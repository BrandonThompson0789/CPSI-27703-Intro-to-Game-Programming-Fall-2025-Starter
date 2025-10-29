#include "PlayerMovementComponent.h"
#include "../Object.h"
#include <iostream>

PlayerMovementComponent::PlayerMovementComponent(Object& parent, float moveSpeed)
    : Component(parent)
    , moveSpeed(moveSpeed)
    , deltaTime(1.0f / 60.0f) // 60 FPS
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

void PlayerMovementComponent::update() {
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
    
    // Calculate velocity
    float velocityX = horizontal * currentSpeed * deltaTime;
    float velocityY = vertical * currentSpeed * deltaTime;
    
    // Get current position
    auto [x, y, angle] = body->getPosition();
    
    // Update position
    body->setPosition(x + velocityX, y + velocityY, angle);
    
    // Debug output for actions
    if (input->isPressed(GameAction::ACTION_INTERACT)) {
        std::cout << "Action Interact pressed!" << std::endl;
    }
    if (input->isPressed(GameAction::ACTION_THROW)) {
        std::cout << "Action Throw pressed!" << std::endl;
    }
}

