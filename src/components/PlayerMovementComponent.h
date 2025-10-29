#ifndef PLAYER_MOVEMENT_COMPONENT_H
#define PLAYER_MOVEMENT_COMPONENT_H

#include "Component.h"
#include "InputComponent.h"
#include "BodyComponent.h"
#include <nlohmann/json.hpp>

class PlayerMovementComponent : public Component {
public:
    PlayerMovementComponent(Object& parent, float moveSpeed = 200.0f);
    PlayerMovementComponent(Object& parent, const nlohmann::json& data);
    virtual ~PlayerMovementComponent() = default;
    
    void update(float deltaTime) override;
    void draw() override {}
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "PlayerMovementComponent"; }
    
    void setMoveSpeed(float speed) { moveSpeed = speed; }
    float getMoveSpeed() const { return moveSpeed; }
    
private:
    InputComponent* input;
    BodyComponent* body;
    float moveSpeed;
};

#endif // PLAYER_MOVEMENT_COMPONENT_H

