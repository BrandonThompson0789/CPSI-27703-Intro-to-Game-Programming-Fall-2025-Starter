#include "BodyComponent.h"
#include "ComponentLibrary.h"

BodyComponent::BodyComponent(Object& parent) : Component(parent) {}

BodyComponent::BodyComponent(Object& parent, const nlohmann::json& data) : Component(parent) {
    if (data.contains("posX")) posX = data["posX"].get<float>();
    if (data.contains("posY")) posY = data["posY"].get<float>();
    if (data.contains("angle")) angle = data["angle"].get<float>();
    if (data.contains("velX")) velX = data["velX"].get<float>();
    if (data.contains("velY")) velY = data["velY"].get<float>();
    if (data.contains("velAngle")) velAngle = data["velAngle"].get<float>();
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

std::tuple<float, float, float> BodyComponent::getPosition() {
    return std::make_tuple(posX, posY, angle);
}

std::tuple<float, float, float> BodyComponent::getVelocity() {
    return std::make_tuple(velX, velY, velAngle);
}

void BodyComponent::update() {
    // Update position based on velocity
    posX += velX;
    posY += velY;
    angle += velAngle;
}

void BodyComponent::draw() {
    // BodyComponent doesn't render anything itself
    // Rendering is typically handled by other components (e.g., SpriteComponent)
}
