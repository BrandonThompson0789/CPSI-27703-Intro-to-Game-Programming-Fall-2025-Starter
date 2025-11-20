#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <string>

class AdjustableComponent : public Component {
public:
    AdjustableComponent(Object& parent);
    AdjustableComponent(Object& parent, const nlohmann::json& data);

    void update(float deltaTime) override;
    void draw() override;
    void use(Object& instigator) override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "AdjustableComponent"; }
    
    // Getters for debug drawing
    const std::string& getGlobalValueName() const { return globalValueName; }
    const std::string& getOperation() const { return operation; }
    float getValue() const { return value; }

private:
    std::string globalValueName;
    std::string operation;  // "set", "add", "subtract", "multiply", "divide"
    float value;
    bool createIfNotExists;
};

