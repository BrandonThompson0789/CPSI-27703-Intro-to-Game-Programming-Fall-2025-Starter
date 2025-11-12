#pragma once

#include "Component.h"
#include "SensorTypes.h"

#include <nlohmann/json.hpp>

class InputComponent;

class InteractComponent : public Component {
public:
    InteractComponent(Object& parent);
    InteractComponent(Object& parent, const nlohmann::json& data);

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "InteractComponent"; }

    bool supportsSense(SenseType sense) const;
    SenseTypeMask getAllowedSenses() const { return allowedSenses; }

    bool isInteractPressed() const;
    float getInteractValue() const;

    float getInteractThreshold() const { return interactThreshold; }
    void setInteractThreshold(float threshold) { interactThreshold = threshold; }
    void setAllowedSenses(SenseTypeMask mask) { allowedSenses = mask; }

private:
    SenseTypeMask allowedSenses;
    float interactThreshold;
    mutable InputComponent* cachedInput;
};

