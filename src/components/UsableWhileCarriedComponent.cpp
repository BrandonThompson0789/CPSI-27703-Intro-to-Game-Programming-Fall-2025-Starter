#include "UsableWhileCarriedComponent.h"
#include "ComponentLibrary.h"
#include "../InputManager.h"
#include "../Engine.h"
#include "behaviors/GrabBehaviorComponent.h"
#include "InputComponent.h"
#include "../Object.h"

UsableWhileCarriedComponent::UsableWhileCarriedComponent(Object& parent)
    : Component(parent)
    , triggerAction(static_cast<int>(GameAction::ACTION_INTERACT))
    , wasActionPressed(false) {
}

UsableWhileCarriedComponent::UsableWhileCarriedComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , triggerAction(static_cast<int>(GameAction::ACTION_INTERACT))
    , wasActionPressed(false) {
    // Support both int (enum value) and string (action name) for triggerAction
    if (data.contains("triggerAction")) {
        if (data["triggerAction"].is_number()) {
            triggerAction = data["triggerAction"].get<int>();
        } else if (data["triggerAction"].is_string()) {
            std::string actionName = data["triggerAction"].get<std::string>();
            GameAction action = InputManager::stringToAction(actionName);
            if (action != GameAction::NUM_ACTIONS) {
                triggerAction = static_cast<int>(action);
            }
        }
    }
}

nlohmann::json UsableWhileCarriedComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    // Serialize as string for readability, but also support int for backwards compatibility
    GameAction action = static_cast<GameAction>(triggerAction);
    if (action != GameAction::NUM_ACTIONS) {
        j["triggerAction"] = InputManager::actionToString(action);
    } else {
        j["triggerAction"] = triggerAction; // Fallback to int if invalid
    }
    return j;
}

void UsableWhileCarriedComponent::update(float deltaTime) {
    (void)deltaTime; // Not used, but required by interface
    
    Object* grabbingObject = findGrabbingObject();
    if (!grabbingObject) {
        wasActionPressed = false;
        return;
    }

    // Get the input component from the grabbing object
    InputComponent* input = grabbingObject->getComponent<InputComponent>();
    if (!input) {
        wasActionPressed = false;
        return;
    }

    // Check if the trigger action is pressed
    GameAction action = static_cast<GameAction>(triggerAction);
    bool actionPressed = input->isPressed(action);

    // If action was just pressed (not held), trigger use
    if (actionPressed && !wasActionPressed) {
        parent().use(*grabbingObject);
    }

    wasActionPressed = actionPressed;
}

Object* UsableWhileCarriedComponent::findGrabbingObject() const {
    Engine* engine = Object::getEngine();
    if (!engine) {
        return nullptr;
    }

    // Iterate through all objects to find which one is grabbing this object
    for (auto& obj : engine->getObjects()) {
        if (!obj) {
            continue;
        }

        GrabBehaviorComponent* grabBehavior = obj->getComponent<GrabBehaviorComponent>();
        if (!grabBehavior) {
            continue;
        }

        // Check if this object is the one being grabbed
        if (grabBehavior->getGrabbedObject() == &parent()) {
            return obj.get();
        }
    }

    return nullptr;
}

// Register component
static ComponentRegistrar<UsableWhileCarriedComponent> registrar("UsableWhileCarriedComponent");

