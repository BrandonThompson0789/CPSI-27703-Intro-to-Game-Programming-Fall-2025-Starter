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
    
    // Allow configurable push force from JSON (kept for future use)
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
    
    // Box2D now handles all collision detection and resolution automatically!
    // The physics engine will prevent objects from overlapping and apply realistic forces.
    
    // This component can now be used for custom box-specific behavior
    // For example:
    // - Special effects when hit
    // - Sound effects on collision
    // - Breaking when hit too hard
    // - etc.
    
    // To detect collisions, you would implement a b2ContactListener in the Engine
    // and register callbacks for specific objects.
    
    // For now, this component is just a placeholder for future box-specific logic.
}
