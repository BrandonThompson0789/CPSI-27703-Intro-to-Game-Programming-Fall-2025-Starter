#include "BodyComponent.h"
#include "ComponentLibrary.h"
#include <iostream>

BodyComponent::BodyComponent(Object& parent) : Component(parent) {}

BodyComponent::BodyComponent(Object& parent, float drag) : Component(parent), drag(drag) {}

BodyComponent::BodyComponent(Object& parent, const nlohmann::json& data) : Component(parent) {
    if (data.contains("posX")) posX = data["posX"].get<float>();
    if (data.contains("posY")) posY = data["posY"].get<float>();
    if (data.contains("angle")) angle = data["angle"].get<float>();
    if (data.contains("velX")) velX = data["velX"].get<float>();
    if (data.contains("velY")) velY = data["velY"].get<float>();
    if (data.contains("velAngle")) velAngle = data["velAngle"].get<float>();
    if (data.contains("drag")) drag = data["drag"].get<float>();
}

nlohmann::json BodyComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["posX"] = posX;
    j["posY"] = posY;
    j["angle"] = angle;
    j["velX"] = velX;
    j["velY"] = velY;
    j["velAngle"] = velAngle;
    j["drag"] = drag;
    return j;
}

// Register this component type with the library
static ComponentRegistrar<BodyComponent> registrar("BodyComponent");

void BodyComponent::setPosition(float x, float y, float angle) {
    posX = x;
    posY = y;
    this->angle = angle;
}

void BodyComponent::setVelocity(float x, float y, float angle) {
    velX = x;
    velY = y;
    velAngle = angle;
}

void BodyComponent::modVelocity(float x, float y, float angle) {
    velX += x;
    velY += y;
    velAngle += angle;
}

std::tuple<float, float, float> BodyComponent::getPosition() {
    return std::make_tuple(posX, posY, angle);
}

std::tuple<float, float, float> BodyComponent::getVelocity() {
    return std::make_tuple(velX, velY, velAngle);
}

void BodyComponent::update(float deltaTime) {
    // Calculate current speed
    float speed = sqrt(velX * velX + velY * velY);
    
    // Apply speed-dependent drag to velocity FIRST (before position integration)
    // Higher speed = more drag for more realistic deceleration
    if (speed > 0.0f) {
        // Calculate drag multiplier based on speed
        // Base drag is applied, with additional drag proportional to speed
        // speedFactor: 0 at rest, increases with speed
        float speedFactor = speed / 800.0f; // Normalize speed (adjust to tune sensitivity)
        
        // Start with base drag, then add extra drag based on speed
        // Example: if drag=0.95 and speedFactor=1.0, dragMult = 0.95 - 0.025 = 0.925
        // If speedFactor=2.0, dragMult = 0.95 - 0.05 = 0.90
        float dragMult = drag - (speedFactor * 0.025f);
        
        // Clamp to prevent negative drag (which would reverse direction)
        if (dragMult < 0.5f) dragMult = 0.5f;
        
        // Apply drag to linear velocity
        velX *= dragMult;
        velY *= dragMult;
        
        // Apply standard drag to angular velocity (no speed dependency)
        velAngle *= drag;
    } else {
        // If no movement, apply standard drag to prevent floating point drift
        velX *= drag;
        velY *= drag;
        velAngle *= drag;
    }
    
    // Update position based on velocity (velocity is already in pixels per second)
    posX += velX * deltaTime;
    posY += velY * deltaTime;
    angle += velAngle * deltaTime;
}

void BodyComponent::draw() {
    // BodyComponent doesn't render anything itself
    // Rendering is typically handled by other components (e.g., SpriteComponent)
}
