#include "BodyComponent.h"

BodyComponent::BodyComponent(Object& parent) : Component(parent) {}

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
