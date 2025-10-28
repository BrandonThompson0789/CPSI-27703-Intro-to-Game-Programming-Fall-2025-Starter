#include "Object.h"
#include "components/Component.h"
#include "components/BodyComponent.h"

Object::~Object() = default;

void Object::update() {
    // Update all components
    for (auto& component : components) {
        component->update();
    }
}

void Object::render(SDL_Renderer* renderer) {
    // Draw all components
    for (auto& component : components) {
        component->draw();
    }
}