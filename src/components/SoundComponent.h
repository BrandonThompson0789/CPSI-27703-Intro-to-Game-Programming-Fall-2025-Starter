#pragma once

#include "Component.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

class BodyComponent;

class SoundComponent : public Component {
public:
    SoundComponent(Object& parent);
    SoundComponent(Object& parent, const nlohmann::json& data);
    ~SoundComponent() override = default;

    void update(float /*deltaTime*/) override {}
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "SoundComponent"; }

    void playActionSound(const std::string& actionName);
    void setActionCollection(const std::string& actionName, const std::string& collection, float volume = 1.0f);

private:
    void initializeFromJson(const nlohmann::json& data);

    struct ActionSound {
        std::string collection;
        float volume = 1.0f;
    };

    void ensureBodyReference();

    std::unordered_map<std::string, ActionSound> actionSounds;
    BodyComponent* bodyComponent = nullptr;
};


