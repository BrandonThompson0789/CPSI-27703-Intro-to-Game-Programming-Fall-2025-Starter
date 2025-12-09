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
    , wasActionPressed(false)
    , autoUse(false)
    , useRate(0.0f)
    , timeSinceLastUse(0.0f) {
}

UsableWhileCarriedComponent::UsableWhileCarriedComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , triggerAction(static_cast<int>(GameAction::ACTION_INTERACT))
    , wasActionPressed(false)
    , autoUse(false)
    , useRate(0.0f)
    , timeSinceLastUse(0.0f) {
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
    
    // Load autoUse setting
    if (data.contains("autoUse")) {
        autoUse = data["autoUse"].get<bool>();
    }
    
    // Load useRate setting
    if (data.contains("useRate")) {
        useRate = data["useRate"].get<float>();
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
    
    // Serialize autoUse and useRate
    if (autoUse) {
        j["autoUse"] = autoUse;
    }
    if (useRate > 0.0f) {
        j["useRate"] = useRate;
    }
    
    return j;
}

void UsableWhileCarriedComponent::update(float deltaTime) {
    Object* grabbingObject = findGrabbingObject();
    if (!grabbingObject) {
        wasActionPressed = false;
        timeSinceLastUse = 0.0f;
        return;
    }

    // Get the input component from the grabbing object
    InputComponent* input = grabbingObject->getComponent<InputComponent>();
    if (!input) {
        wasActionPressed = false;
        timeSinceLastUse = 0.0f;
        return;
    }

    // Update rate limiting timer
    timeSinceLastUse += deltaTime;

    // Check if the trigger action is pressed
    GameAction action = static_cast<GameAction>(triggerAction);
    bool actionPressed = input->isPressed(action);

    // Determine if we should trigger use
    bool shouldUse = false;
    if (autoUse) {
        // In autoUse mode, use continuously while action is held (respecting useRate)
        if (actionPressed && (useRate <= 0.0f || timeSinceLastUse >= useRate)) {
            shouldUse = true;
        }
    } else {
        // In normal mode, only use when action is first pressed (respecting useRate)
        if (actionPressed && !wasActionPressed && (useRate <= 0.0f || timeSinceLastUse >= useRate)) {
            shouldUse = true;
        }
    }

    // Trigger use if conditions are met
    if (shouldUse) {
        parent().use(*grabbingObject);
        timeSinceLastUse = 0.0f; // Reset rate limiting timer
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

