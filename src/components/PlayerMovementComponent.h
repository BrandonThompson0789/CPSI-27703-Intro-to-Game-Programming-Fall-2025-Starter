#ifndef PLAYER_MOVEMENT_COMPONENT_H
#define PLAYER_MOVEMENT_COMPONENT_H

#include "Component.h"
#include "InputComponent.h"
#include "BodyComponent.h"
#include "SpriteComponent.h"
#include "JointComponent.h"
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
    
    // Grab/throw settings
    void setGrabDistance(float distance) { grabDistance = distance; }
    void setGrabForce(float force) { grabForce = force; }
    void setBreakForce(float force) { breakForce = force; }
    void setMaxThrowCharge(float time) { maxThrowCharge = time; }
    void setThrowForceMultiplier(float multiplier) { throwForceMultiplier = multiplier; }
    
private:
    // Components
    InputComponent* input;
    BodyComponent* body;
    SpriteComponent* sprite;
    
    // Movement
    float moveSpeed;
    
    // Grab/throw system
    Object* grabbedObject;          // Currently held object
    JointComponent* grabJoint;      // Joint holding the object
    float throwChargeTime;          // Time spent charging throw
    bool isChargingThrow;           // Whether currently charging a throw
    bool wasThrowPressed;           // Previous frame throw button state
    bool wasInteractPressed;        // Previous frame interact button state
    
    // Grab/throw parameters
    float grabDistance;             // How far in front to check for objects (pixels)
    float grabForce;                // Motor joint max force
    float breakForce;               // Force at which grab breaks
    float maxThrowCharge;           // Max charge time (seconds)
    float throwForceMultiplier;     // Force multiplier for throwing
    
    // Helper methods
    Object* findGrabbableObject();
    void grabObject(Object* obj);
    void releaseObject();
    void throwObject(float chargeRatio);
    void updateGrab(float deltaTime);
    void updateThrow(float deltaTime);
};

#endif // PLAYER_MOVEMENT_COMPONENT_H

