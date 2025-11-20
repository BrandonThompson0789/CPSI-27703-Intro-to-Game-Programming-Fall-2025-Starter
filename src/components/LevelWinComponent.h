#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <string>

class LevelWinComponent : public Component {
public:
    LevelWinComponent(Object& parent);
    LevelWinComponent(Object& parent, const nlohmann::json& data);

    void update(float deltaTime) override;
    void draw() override;
    void use(Object& instigator) override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "LevelWinComponent"; }

private:
    std::string targetLevelName;  // If empty, loads next available level
    bool updateProgression;
    int progressionIncrement;  // How much to increment progression by
};

