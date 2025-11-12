#include "InteractComponent.h"

#include "ComponentLibrary.h"
#include "InputComponent.h"

#include "../Object.h"

InteractComponent::InteractComponent(Object& parent)
    : Component(parent)
    , allowedSenses(addSenseToMask(addSenseToMask(makeSenseMask(SenseType::None), SenseType::Collision), SenseType::Distance))
    , interactThreshold(0.5f)
    , cachedInput(nullptr) {
    allowedSenses = addSenseToMask(allowedSenses, SenseType::InteractInput);
}

InteractComponent::InteractComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , allowedSenses(makeSenseMask(SenseType::None))
    , interactThreshold(data.value("interactThreshold", 0.5f))
    , cachedInput(nullptr) {
    if (data.contains("allowedSenses") && data["allowedSenses"].is_array()) {
        allowedSenses = senseMaskFromStrings(data["allowedSenses"].get<std::vector<std::string>>());
    } else {
        // Default to all senses enabled
        allowedSenses = addSenseToMask(makeSenseMask(SenseType::None), SenseType::Collision);
        allowedSenses = addSenseToMask(allowedSenses, SenseType::Distance);
        allowedSenses = addSenseToMask(allowedSenses, SenseType::InteractInput);
    }
}

void InteractComponent::update(float /*deltaTime*/) {
    if (!cachedInput) {
        cachedInput = parent().getComponent<InputComponent>();
    }
}

bool InteractComponent::supportsSense(SenseType sense) const {
    return senseMaskHas(allowedSenses, sense);
}

float InteractComponent::getInteractValue() const {
    if (!cachedInput) {
        auto* input = parent().getComponent<InputComponent>();
        if (input) {
            cachedInput = input;
        }
    }
    if (!cachedInput) {
        return 0.0f;
    }
    return cachedInput->getInput(GameAction::ACTION_INTERACT);
}

bool InteractComponent::isInteractPressed() const {
    return getInteractValue() >= interactThreshold;
}

nlohmann::json InteractComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["allowedSenses"] = senseMaskToStrings(allowedSenses);
    data["interactThreshold"] = interactThreshold;
    return data;
}

void InteractComponent::draw() {
    // Intentionally empty; interaction component has no visual representation.
}

static ComponentRegistrar<InteractComponent> registrar("InteractComponent");

