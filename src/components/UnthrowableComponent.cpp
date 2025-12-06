#include "UnthrowableComponent.h"
#include "ComponentLibrary.h"

UnthrowableComponent::UnthrowableComponent(Object& parent)
    : Component(parent) {
}

UnthrowableComponent::UnthrowableComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    (void)data; // No parameters needed for this marker component
}

nlohmann::json UnthrowableComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    return j;
}

// Register component
static ComponentRegistrar<UnthrowableComponent> registrar("UnthrowableComponent");

