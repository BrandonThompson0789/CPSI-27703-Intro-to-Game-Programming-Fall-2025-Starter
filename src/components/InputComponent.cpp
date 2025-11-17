#include "InputComponent.h"
#include "ComponentLibrary.h"
#include <algorithm>

InputComponent::InputComponent(Object& parent, int playerId)
    : Component(parent)
    , playerId(playerId)
    , playerManager(PlayerManager::getInstance())
{
}

InputComponent::InputComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , playerManager(PlayerManager::getInstance())
{
    // Get playerId from JSON (defaults to 1 if not specified)
    if (data.contains("playerId")) {
        playerId = data["playerId"].get<int>();
    } else {
        // Default to player ID 1 (keyboard is assigned to player 1 by default)
        playerId = 1;
    }
    
    // Set config name in PlayerManager if provided
    if (data.contains("configName")) {
        std::string configName = data["configName"].get<std::string>();
        playerManager.setPlayerConfigName(playerId, configName);
    }
}

nlohmann::json InputComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["playerId"] = playerId;
    std::string configName = playerManager.getPlayerConfigName(playerId);
    if (!configName.empty()) {
        j["configName"] = configName;
    }
    return j;
}

// Register this component type with the library
static ComponentRegistrar<InputComponent> registrar("InputComponent");

void InputComponent::update(float deltaTime) {
    // Input is updated by InputManager in Engine
    // This component just provides easy access to the data
    // deltaTime parameter is ignored
}

float InputComponent::getInput(GameAction action) const {
    // PlayerManager will use the player's config name if set
    return playerManager.getInputValue(playerId, action, "");
}

std::string InputComponent::getConfigName() const {
    return playerManager.getPlayerConfigName(playerId);
}

void InputComponent::setConfigName(const std::string& name) {
    playerManager.setPlayerConfigName(playerId, name);
}

bool InputComponent::isPressed(GameAction action) const {
    return getInput(action) > 0.5f;
}

bool InputComponent::isActive() const {
    if (!playerManager.isPlayerAssigned(playerId)) {
        return false;
    }
    
    // Check if player has any active input
    // Check a few common actions to see if there's any input
    float totalInput = 0.0f;
    totalInput += getInput(GameAction::MOVE_UP);
    totalInput += getInput(GameAction::MOVE_DOWN);
    totalInput += getInput(GameAction::MOVE_LEFT);
    totalInput += getInput(GameAction::MOVE_RIGHT);
    totalInput += getInput(GameAction::ACTION_WALK);
    totalInput += getInput(GameAction::ACTION_INTERACT);
    totalInput += getInput(GameAction::ACTION_THROW);
    
    return totalInput > 0.0f;
}

