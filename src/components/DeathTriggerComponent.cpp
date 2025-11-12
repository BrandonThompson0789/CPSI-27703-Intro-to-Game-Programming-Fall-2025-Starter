#include "DeathTriggerComponent.h"

#include "ComponentLibrary.h"
#include "../Object.h"

#include <iostream>
DeathTriggerComponent::DeathTriggerComponent(Object& parent)
    : Component(parent) {
}

DeathTriggerComponent::DeathTriggerComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    // Currently no configuration needed, but we accept JSON for consistency
    (void)data;
}

void DeathTriggerComponent::update(float deltaTime) {
    // Nothing to update
    (void)deltaTime;
}

void DeathTriggerComponent::draw() {
    // Nothing to draw
}

void DeathTriggerComponent::use(Object& instigator) {
    // Mark the parent object for death when triggered
    (void)instigator; // We don't need the instigator for this simple component
    std::cout << "[DeathTriggerComponent] DeathTriggerComponent::use called for object: " << parent().getName() << std::endl;
    parent().markForDeath();
}

nlohmann::json DeathTriggerComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    return j;
}

// Register this component type with the library
static ComponentRegistrar<DeathTriggerComponent> registrar("DeathTriggerComponent");

