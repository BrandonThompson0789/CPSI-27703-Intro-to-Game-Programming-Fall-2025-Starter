#pragma once

#include "../Component.h"
#include "../InputComponent.h"
#include "../BodyComponent.h"
#include <nlohmann/json.hpp>

class GrabBehaviorComponent;
class SoundComponent;

/**
 * Handles charging and throwing of objects currently held by the grab
 * behaviour.
 */
class ThrowBehaviorComponent : public Component {
public:
    ThrowBehaviorComponent(Object& parent);
    ThrowBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~ThrowBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "ThrowBehaviorComponent"; }

    void setMaxThrowCharge(float charge) { maxThrowCharge = charge; }
    void setThrowForceMultiplier(float multiplier) { throwForceMultiplier = multiplier; }

private:
    void resolveDependencies();
    void updateChargeState(float deltaTime);
    void executeThrow(float chargeRatio);

    InputComponent* input;
    BodyComponent* body;
    GrabBehaviorComponent* grabBehavior;
    SoundComponent* soundComponent;

    float maxThrowCharge;
    float throwForceMultiplier;
    float minThrowForceRatio;

    float throwChargeTime;
    bool isChargingThrow;
    bool wasThrowPressed;
};


