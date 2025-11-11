#include "SoundComponent.h"

#include "BodyComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include "../SoundManager.h"

#include <algorithm>
#include <iostream>

SoundComponent::SoundComponent(Object& parent)
    : Component(parent) {
    ensureBodyReference();
}

SoundComponent::SoundComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    initializeFromJson(data);
    ensureBodyReference();
}

void SoundComponent::initializeFromJson(const nlohmann::json& data) {
    if (!data.contains("collections")) {
        return;
    }

    const auto& collectionsNode = data["collections"];
    if (!collectionsNode.is_object()) {
        std::cerr << "SoundComponent::initializeFromJson: 'collections' must be an object" << std::endl;
        return;
    }

    for (const auto& [rawActionName, value] : collectionsNode.items()) {
        std::string actionName = rawActionName;
        if (actionName == "move_start") {
            actionName = "move";
        }

        ActionSound config;

        if (value.is_string()) {
            config.collection = value.get<std::string>();
        } else if (value.is_object()) {
            config.collection = value.value("collection", "");
            config.volume = value.value("volume", 1.0f);
        } else {
            std::cerr << "SoundComponent::initializeFromJson: Invalid entry for action '" << actionName << "'" << std::endl;
            continue;
        }

        if (config.collection.empty()) {
            std::cerr << "SoundComponent::initializeFromJson: Missing collection name for action '" << actionName << "'" << std::endl;
            continue;
        }

        config.volume = std::clamp(config.volume, 0.0f, 1.0f);
        actionSounds[actionName] = config;
    }
}

nlohmann::json SoundComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();

    nlohmann::json collectionsNode = nlohmann::json::object();
    for (const auto& [action, config] : actionSounds) {
        nlohmann::json entry;
        entry["collection"] = config.collection;
        entry["volume"] = config.volume;
        collectionsNode[action] = entry;
    }

    data["collections"] = collectionsNode;
    return data;
}

void SoundComponent::ensureBodyReference() {
    if (!bodyComponent) {
        bodyComponent = parent().getComponent<BodyComponent>();
    }
}

void SoundComponent::playActionSound(const std::string& actionName) {
    auto it = actionSounds.find(actionName);
    if (it == actionSounds.end()) {
        return;
    }

    SoundManager& soundManager = SoundManager::getInstance();
    if (!soundManager.isInitialized()) {
        return;
    }

    ensureBodyReference();

    if (bodyComponent) {
        auto [worldX, worldY, angleDegrees] = bodyComponent->getPosition();
        (void)angleDegrees;
        soundManager.playCollectionAt(it->second.collection, worldX, worldY, it->second.volume);
    } else {
        soundManager.playCollection(it->second.collection, it->second.volume);
    }
}

void SoundComponent::setActionCollection(const std::string& actionName, const std::string& collection, float volume) {
    if (collection.empty()) {
        actionSounds.erase(actionName);
        return;
    }

    ActionSound config;
    config.collection = collection;
    config.volume = std::clamp(volume, 0.0f, 1.0f);
    actionSounds[actionName] = config;
}

static ComponentRegistrar<SoundComponent> registrar("SoundComponent");


