#include "BoxBehaviorComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include "../Engine.h"
#include "BodyComponent.h"
#include <cmath>
#include <iostream>

BoxBehaviorComponent::BoxBehaviorComponent(Object& parent)
    : Component(parent)
{
    // Get required BodyComponent
    body = parent.getComponent<BodyComponent>();
    
    if (!body) {
        std::cerr << "Warning: BoxBehaviorComponent requires BodyComponent!" << std::endl;
    }
}

BoxBehaviorComponent::BoxBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
{
    // Get required BodyComponent
    body = parent.getComponent<BodyComponent>();
    
    if (!body) {
        std::cerr << "Warning: BoxBehaviorComponent requires BodyComponent!" << std::endl;
    }
    
    // Allow configurable push force from JSON
    if (data.contains("pushForce")) {
        pushForce = data["pushForce"].get<float>();
    }
}

nlohmann::json BoxBehaviorComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["pushForce"] = pushForce;
    return j;
}

// Register this component type with the library
static ComponentRegistrar<BoxBehaviorComponent> registrar("BoxBehaviorComponent");

void BoxBehaviorComponent::update(float deltaTime) {
    if (!body) return;
    
    // Get the Engine instance to access other objects
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    auto& objects = engine->getObjects();
    
    auto [posX, posY, angle] = body->getPosition();
    auto [velX, velY, velAngle] = body->getVelocity();
    
    // Simple size estimation (assuming 32x32 sprite for now)
    float halfWidth = 16.0f;
    float halfHeight = 16.0f;
    
    for (auto& obj : objects) {
        // Don't collide with self
        if (obj.get() == &parent()) continue;
        
        // Get the other object's BodyComponent
        BodyComponent* other = obj->getComponent<BodyComponent>();
        if (!other) continue;
        
        auto [otherX, otherY, otherAngle] = other->getPosition();
        auto [otherVelX, otherVelY, otherVelAngle] = other->getVelocity();
        
        // Simple AABB collision detection
        // Check if rectangles overlap
        if (std::abs(posX - otherX) < (halfWidth + halfHeight) && 
            std::abs(posY - otherY) < (halfWidth + halfHeight)) {
            
            // Calculate direction from other object to this box
            float dx = posX - otherX;
            float dy = posY - otherY;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance > 0.0f) {
                // Normalize direction
                dx /= distance;
                dy /= distance;
                
                // Apply push force in the direction away from the other object
                // Only apply if the other object is moving
                float otherSpeed = std::sqrt(otherVelX * otherVelX + otherVelY * otherVelY);
                if (otherSpeed > 10.0f) { // Only push if other object is moving meaningfully
                    // Scale push force based on other object's speed
                    float forceScale = std::min(otherSpeed / 200.0f, 1.0f); // Scale 0-1 based on speed
                    
                    // Apply velocity in the push direction
                    float pushVelX = dx * pushForce * forceScale;
                    float pushVelY = dy * pushForce * forceScale;
                    
                    body->modVelocity(pushVelX, pushVelY, 0.0f);
                }
            }
        }
    }
}
