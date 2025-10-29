#ifndef PLAYER_MOVEMENT_COMPONENT_H
#define PLAYER_MOVEMENT_COMPONENT_H

#include "Component.h"
#include "InputComponent.h"
#include "BodyComponent.h"

class PlayerMovementComponent : public Component {
public:
    PlayerMovementComponent(Object& parent, float moveSpeed = 200.0f);
    virtual ~PlayerMovementComponent() = default;
    
    void update() override;
    void draw() override {}
    
    void setMoveSpeed(float speed) { moveSpeed = speed; }
    float getMoveSpeed() const { return moveSpeed; }
    
private:
    InputComponent* input;
    BodyComponent* body;
    float moveSpeed;
    float deltaTime;
};

#endif // PLAYER_MOVEMENT_COMPONENT_H

