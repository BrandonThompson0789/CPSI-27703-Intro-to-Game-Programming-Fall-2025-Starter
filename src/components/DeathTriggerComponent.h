#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>

class Object;

class DeathTriggerComponent : public Component {
public:
    explicit DeathTriggerComponent(Object& parent);
    DeathTriggerComponent(Object& parent, const nlohmann::json& data);
    ~DeathTriggerComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;
    void use(Object& instigator) override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "DeathTriggerComponent"; }
};

