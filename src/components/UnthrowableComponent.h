#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>

/**
 * Marker component that prevents an object from being thrown.
 * When attached to an object, the ThrowBehaviorComponent will
 * not be able to throw this object, even if it is grabbed.
 */
class UnthrowableComponent : public Component {
public:
    UnthrowableComponent(Object& parent);
    UnthrowableComponent(Object& parent, const nlohmann::json& data);
    ~UnthrowableComponent() override = default;

    void update(float deltaTime) override {}
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "UnthrowableComponent"; }
};

