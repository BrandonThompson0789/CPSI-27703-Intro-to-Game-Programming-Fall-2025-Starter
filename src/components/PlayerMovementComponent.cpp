#include "PlayerMovementComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include "../Engine.h"
#include <iostream>
#include <cmath>
#include <box2d/box2d.h>

PlayerMovementComponent::PlayerMovementComponent(Object& parent, float moveSpeed)
    : Component(parent)
    , moveSpeed(moveSpeed)
    , grabbedObject(nullptr)
    , grabJoint(nullptr)
    , throwChargeTime(0.0f)
    , isChargingThrow(false)
    , wasThrowPressed(false)
    , wasInteractPressed(false)
    , grabDistance(80.0f)
    , grabForce(200.0f)
    , breakForce(500.0f)
    , maxThrowCharge(1.0f)
    , throwForceMultiplier(1000.0f)
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
    , grabbedObject(nullptr)
    , grabJoint(nullptr)
    , throwChargeTime(0.0f)
    , isChargingThrow(false)
    , wasThrowPressed(false)
    , wasInteractPressed(false)
    , grabDistance(32.0f)
    , grabForce(200.0f)
    , breakForce(500.0f)
    , maxThrowCharge(1.0f)
    , throwForceMultiplier(1000.0f)
{
    if (data.contains("moveSpeed")) moveSpeed = data["moveSpeed"].get<float>();
    if (data.contains("grabDistance")) grabDistance = data["grabDistance"].get<float>();
    if (data.contains("grabForce")) grabForce = data["grabForce"].get<float>();
    if (data.contains("breakForce")) breakForce = data["breakForce"].get<float>();
    if (data.contains("maxThrowCharge")) maxThrowCharge = data["maxThrowCharge"].get<float>();
    if (data.contains("throwForceMultiplier")) throwForceMultiplier = data["throwForceMultiplier"].get<float>();
    
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
    j["grabDistance"] = grabDistance;
    j["grabForce"] = grabForce;
    j["breakForce"] = breakForce;
    j["maxThrowCharge"] = maxThrowCharge;
    j["throwForceMultiplier"] = throwForceMultiplier;
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
    
    // Update grab/throw system first
    updateGrab(deltaTime);
    updateThrow(deltaTime);
    
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
}

void PlayerMovementComponent::updateGrab(float deltaTime) {
    if (!input || !body) return;
    
    bool interactPressed = input->isPressed(GameAction::ACTION_INTERACT);
    
    // Check if grab joint broke
    if (grabbedObject && grabJoint) {
        if (grabJoint->isJointBroken()) {
            releaseObject();  // Use proper release instead of just clearing
            return;
        }
    }
    
    // Update grab position to keep object in front of player when using motor joint
    if (grabbedObject && grabJoint && B2_IS_NON_NULL(grabJoint->getJointId())) {
        b2JointId jointId = grabJoint->getJointId();
        
        // Only update motor joints
        if (b2Joint_IsValid(jointId) && b2Joint_GetType(jointId) == b2_motorJoint) {
            // Get player's current angle
            auto [playerX, playerY, playerAngle] = body->getPosition();
            
            // Get object's current position
            BodyComponent* objBody = grabbedObject->getComponent<BodyComponent>();
            if (objBody) {
                auto [objX, objY, objAngle] = objBody->getPosition();
                
                // Calculate current distance to maintain it
                float dx = objX - playerX;
                float dy = objY - playerY;
                float holdDistance = std::sqrt(dx * dx + dy * dy);
                
                // Calculate direction player is facing
                float dirX = std::sin(playerAngle);
                float dirY = -std::cos(playerAngle);
                
                // Set offset to be in front at the current distance
                b2Vec2 newOffset = {
                    (dirX * holdDistance) * Engine::PIXELS_TO_METERS,
                    (dirY * holdDistance) * Engine::PIXELS_TO_METERS
                };
                
                b2MotorJoint_SetLinearOffset(jointId, newOffset);
            }
        }
    }
    
    // Detect button press (edge detection - only trigger on transition)
    if (interactPressed && !wasInteractPressed) {
        if (!grabbedObject) {
            // Try to grab an object
            Object* target = findGrabbableObject();
            if (target) {
                grabObject(target);
            }
        } else {
            // Release currently held object
            releaseObject();
        }
    }
    
    // Update button state for next frame
    wasInteractPressed = interactPressed;
}

void PlayerMovementComponent::updateThrow(float deltaTime) {
    if (!input || !body || !grabbedObject) {
        throwChargeTime = 0.0f;
        isChargingThrow = false;
        wasThrowPressed = false;
        return;
    }
    
    bool throwPressed = input->isPressed(GameAction::ACTION_THROW);
    
    // Detect button press (transition from not pressed to pressed)
    if (throwPressed && !wasThrowPressed) {
        // Start charging throw
        isChargingThrow = true;
        throwChargeTime = 0.0f;
    }
    
    // Update charge time while button is held
    if (throwPressed && isChargingThrow) {
        throwChargeTime += deltaTime;
        if (throwChargeTime > maxThrowCharge) {
            throwChargeTime = maxThrowCharge;
        }
        
        // Visual feedback (optional)
        if ((int)(throwChargeTime * 10) % 2 == 0) {
            // Could flash sprite or show charge indicator
        }
    }
    
    // Detect button release (transition from pressed to not pressed)
    if (!throwPressed && wasThrowPressed && isChargingThrow) {
        // Throw the object!
        float chargeRatio = throwChargeTime / maxThrowCharge;
        throwObject(chargeRatio);
        isChargingThrow = false;
        throwChargeTime = 0.0f;
    }
    
    wasThrowPressed = throwPressed;
}

Object* PlayerMovementComponent::findGrabbableObject() {
    if (!body) return nullptr;
    
    Engine* engine = Object::getEngine();
    if (!engine) return nullptr;
    
    // Get player position and facing direction
    auto [playerX, playerY, playerAngle] = body->getPosition();
    
    // Calculate direction vector from player's angle
    float dirX = std::sin(playerAngle);
    float dirY = -std::cos(playerAngle);
    
    // Raycast start point (slightly in front of player to avoid self-collision)
    float startOffset = 10.0f;
    float rayStartX = playerX + dirX * startOffset;
    float rayStartY = playerY + dirY * startOffset;
    
    b2Vec2 rayOrigin = {
        rayStartX * Engine::PIXELS_TO_METERS, 
        rayStartY * Engine::PIXELS_TO_METERS
    };
    
    // Calculate the translation vector (direction * distance)
    float rayDistance = grabDistance - startOffset;
    b2Vec2 rayTranslation = {
        dirX * rayDistance * Engine::PIXELS_TO_METERS,
        dirY * rayDistance * Engine::PIXELS_TO_METERS
    };
    
    // Perform raycast
    b2WorldId worldId = engine->getPhysicsWorld();
    b2QueryFilter filter = b2DefaultQueryFilter();
    b2RayResult result = b2World_CastRayClosest(worldId, rayOrigin, rayTranslation, filter);
    
    if (result.hit) {
        // Check if shape ID is valid
        if (B2_IS_NULL(result.shapeId)) {
            return nullptr;
        }
        
        // Get the body that was hit
        b2BodyId hitBodyId = b2Shape_GetBody(result.shapeId);
        void* userData = b2Body_GetUserData(hitBodyId);
        
        if (userData) {
            Object* hitObject = static_cast<Object*>(userData);
            
            // Don't grab ourselves
            if (hitObject == &parent()) {
                return nullptr;
            }
            
            // Check if object has a dynamic body (can be grabbed)
            BodyComponent* targetBody = hitObject->getComponent<BodyComponent>();
            if (!targetBody) {
                return nullptr;
            }
            
            b2BodyType bodyType = b2Body_GetType(targetBody->getBodyId());
            if (bodyType != b2_dynamicBody) {
                return nullptr;
            }
            
            return hitObject;
        }
    }
    
    return nullptr;
}

void PlayerMovementComponent::grabObject(Object* obj) {
    if (!obj || !body) return;
    
    grabbedObject = obj;
    
    // Get player and object positions
    auto [playerX, playerY, playerAngle] = body->getPosition();
    
    BodyComponent* objBody = obj->getComponent<BodyComponent>();
    if (!objBody) return;
    
    auto [objX, objY, objAngle] = objBody->getPosition();
    
    // Calculate current offset and distance
    float dx = objX - playerX;
    float dy = objY - playerY;
    float holdDistance = std::sqrt(dx * dx + dy * dy);
    
    // Calculate direction player is facing
    float dirX = std::sin(playerAngle);
    float dirY = -std::cos(playerAngle);
    
    // Create offset in the facing direction at the current distance
    b2Vec2 grabOffset = {
        (dirX * holdDistance) * Engine::PIXELS_TO_METERS,
        (dirY * holdDistance) * Engine::PIXELS_TO_METERS
    };
    
    // Calculate relative rotation (preserve object's current rotation relative to player)
    float angularOffset = objAngle - playerAngle;
    // Normalize to [-π, π]
    while (angularOffset > M_PI) angularOffset -= 2.0f * M_PI;
    while (angularOffset < -M_PI) angularOffset += 2.0f * M_PI;
    
    // Reuse existing JointComponent or create a new one
    if (!grabJoint) {
        grabJoint = parent().addComponent<JointComponent>();
    }
    
    // Use motor joint with preserved angular offset
    grabJoint->createMotorJoint(
        grabbedObject,
        grabOffset,      // Offset in facing direction
        angularOffset,   // Preserve current rotation difference
        grabForce * 3.0f,  // Higher force to overcome player movement
        50.0f           // Max torque
    );
    
    // Set breaking limit
    grabJoint->setBreakForce(breakForce);
}

void PlayerMovementComponent::releaseObject() {
    if (!grabbedObject) return;
    
    // Destroy the grab joint (but keep the component to reuse it)
    if (grabJoint) {
        grabJoint->destroyJoint();
        // DON'T set grabJoint = nullptr - we want to reuse this component!
    }
    
    grabbedObject = nullptr;
}

void PlayerMovementComponent::throwObject(float chargeRatio) {
    if (!grabbedObject || !body) return;
    
    // Save a reference to the object before releasing
    Object* objectToThrow = grabbedObject;
    
    // Get player facing direction
    auto [playerX, playerY, playerAngle] = body->getPosition();
    // Player angle system: 0 = North, π/2 = East, π = South, -π/2 = West
    float dirX = std::sin(playerAngle);
    float dirY = -std::cos(playerAngle);
    
    // Calculate throw force based on charge
    float throwForce = throwForceMultiplier * (0.3f + chargeRatio * 0.7f); // Min 30%, max 100%
    
    // Release the grab joint
    releaseObject();
    
    // Apply impulse to the object (using saved reference)
    BodyComponent* targetBody = objectToThrow->getComponent<BodyComponent>();
    if (targetBody) {
        // Get current velocity to add to it
        auto [currentVelX, currentVelY, currentVelAngle] = targetBody->getVelocity();
        
        // Calculate throw velocity
        float throwVelX = dirX * throwForce;
        float throwVelY = dirY * throwForce;
        
        // Set the velocity (includes current velocity plus throw)
        targetBody->setVelocity(
            currentVelX + throwVelX,
            currentVelY + throwVelY,
            0.0f
        );
    }
}
