#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>

class InputComponent;
class GrabBehaviorComponent;

/**
 * Component that allows an object to be 'used' (triggers Object::use()) by
 * the object carrying it when a specified input action is pressed.
 * 
 * The component checks each frame if this object is being grabbed by another
 * object, and if so, monitors the grabbing object's input for the specified
 * action. When the action is pressed, it calls use() on this object with the
 * grabbing object as the instigator.
 */
class UsableWhileCarriedComponent : public Component {
public:
    UsableWhileCarriedComponent(Object& parent);
    UsableWhileCarriedComponent(Object& parent, const nlohmann::json& data);
    ~UsableWhileCarriedComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "UsableWhileCarriedComponent"; }

    // Get/set the action that triggers use
    int getTriggerAction() const { return triggerAction; }
    void setTriggerAction(int action) { triggerAction = action; }

    // Get/set autofire/autouse mode
    bool getAutoUse() const { return autoUse; }
    void setAutoUse(bool enabled) { autoUse = enabled; }

    // Get/set use rate (time in seconds between uses, 0 = no rate limiting)
    float getUseRate() const { return useRate; }
    void setUseRate(float rate) { useRate = rate; }

private:
    Object* findGrabbingObject() const;
    
    int triggerAction; // GameAction enum value (as int for JSON serialization)
    bool wasActionPressed; // Track previous frame's state to detect press events
    bool autoUse; // If true, continuously use while action is held; if false, only on press
    float useRate; // Minimum time in seconds between uses (0 = no rate limiting)
    float timeSinceLastUse; // Timer to track rate limiting
};

