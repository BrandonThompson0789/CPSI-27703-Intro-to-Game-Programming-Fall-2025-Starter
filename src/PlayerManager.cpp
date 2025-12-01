#include "PlayerManager.h"
#include <algorithm>
#include <iostream>

PlayerManager& PlayerManager::getInstance() {
    static PlayerManager instance;
    return instance;
}

PlayerManager::PlayerManager() 
    : inputManager(InputManager::getInstance()) {
}

bool PlayerManager::assignInputDevice(int playerId, int inputSource) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    // First, remove this input source from any other LOCAL player that might have it
    // (to ensure each input device is only assigned to one local player)
    // Don't remove from network players - they have their controllers on the client/host side
    for (auto& [otherPlayerId, otherPlayer] : players) {
        if (otherPlayerId != playerId && otherPlayer.isLocal) {
            auto it = std::find(otherPlayer.inputDevices.begin(), otherPlayer.inputDevices.end(), inputSource);
            if (it != otherPlayer.inputDevices.end()) {
                otherPlayer.inputDevices.erase(it);
                std::cout << "PlayerManager: Removed input source " << inputSource 
                         << " from local player " << otherPlayerId << " (reassigning to player " << playerId << ")" << std::endl;
            }
        }
    }
    
    PlayerInput& player = players[playerId];
    player.isLocal = true;
    player.networkId.clear();
    
    // Add input source if not already present
    if (std::find(player.inputDevices.begin(), player.inputDevices.end(), inputSource) == player.inputDevices.end()) {
        player.inputDevices.push_back(inputSource);
    }
    
    return true;
}

bool PlayerManager::assignInputDevices(int playerId, const std::vector<int>& inputSources) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    // First, remove all these input sources from any other LOCAL players
    // (to ensure each input device is only assigned to one local player)
    // Don't remove from network players - they have their controllers on the client/host side
    for (int inputSource : inputSources) {
        for (auto& [otherPlayerId, otherPlayer] : players) {
            if (otherPlayerId != playerId && otherPlayer.isLocal) {
                auto it = std::find(otherPlayer.inputDevices.begin(), otherPlayer.inputDevices.end(), inputSource);
                if (it != otherPlayer.inputDevices.end()) {
                    otherPlayer.inputDevices.erase(it);
                    std::cout << "PlayerManager: Removed input source " << inputSource 
                             << " from local player " << otherPlayerId << " (reassigning to player " << playerId << ")" << std::endl;
                }
            }
        }
    }
    
    PlayerInput& player = players[playerId];
    player.isLocal = true;
    player.networkId.clear();
    player.inputDevices = inputSources;
    
    // Remove duplicates
    std::sort(player.inputDevices.begin(), player.inputDevices.end());
    player.inputDevices.erase(
        std::unique(player.inputDevices.begin(), player.inputDevices.end()),
        player.inputDevices.end()
    );
    
    return true;
}

bool PlayerManager::assignNetworkId(int playerId, const std::string& networkId) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    PlayerInput& player = players[playerId];
    
    // Don't assign network ID to a player that has local input devices
    // (local players should keep their controllers/keyboard)
    if (!player.inputDevices.empty()) {
        std::cerr << "PlayerManager: Warning - Cannot assign network ID to player " << playerId 
                 << " because it has local input devices" << std::endl;
        return false;
    }
    
    player.isLocal = false;
    player.networkId = networkId;
    player.inputDevices.clear(); // Should already be empty, but clear just in case
    
    // Initialize network input values to 0
    player.networkInputActive = false;
    for (size_t i = 0; i < static_cast<size_t>(GameAction::NUM_ACTIONS); ++i) {
        player.networkInputValues[i] = 0.0f;
    }
    
    return true;
}

void PlayerManager::unassignPlayer(int playerId) {
    std::lock_guard<std::mutex> lock(playersMutex);
    players.erase(playerId);
}

float PlayerManager::getInputValue(int playerId, GameAction action, const std::string& configName) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    auto it = players.find(playerId);
    if (it == players.end()) {
        return 0.0f;
    }
    
    const PlayerInput& player = it->second;
    
    // If network input is active, use that
    if (player.networkInputActive) {
        // Map GameAction to network input array index
        int index = -1;
        switch (action) {
            case GameAction::MOVE_UP:
                index = 0;
                break;
            case GameAction::MOVE_DOWN:
                index = 1;
                break;
            case GameAction::MOVE_LEFT:
                index = 2;
                break;
            case GameAction::MOVE_RIGHT:
                index = 3;
                break;
            case GameAction::ACTION_WALK:
                index = 4;
                break;
            case GameAction::ACTION_INTERACT:
                index = 5;
                break;
            case GameAction::ACTION_THROW:
                index = 6;
                break;
            default:
                return 0.0f;
        }
        if (index >= 0 && index < static_cast<int>(GameAction::NUM_ACTIONS)) {
            return player.networkInputValues[index];
        }
        return 0.0f;
    }
    
    // Use local input devices
    if (!player.isLocal || player.inputDevices.empty()) {
        return 0.0f;
    }
    
    // Use player's config name if set, otherwise use the passed configName
    std::string configToUse = player.configName.empty() ? configName : player.configName;
    
    // Return the highest value from all input devices
    float maxValue = 0.0f;
    for (int source : player.inputDevices) {
        float value = inputManager.getInputValue(source, action, configToUse);
        if (value > maxValue) {
            maxValue = value;
        }
    }
    return maxValue;
}

bool PlayerManager::isPlayerAssigned(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    return players.find(playerId) != players.end();
}

bool PlayerManager::isPlayerLocal(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end()) {
        return false;
    }
    return it->second.isLocal;
}

std::vector<int> PlayerManager::getPlayerInputDevices(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end() || !it->second.isLocal) {
        return {};
    }
    return it->second.inputDevices;
}

std::string PlayerManager::getPlayerNetworkId(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end() || it->second.isLocal) {
        return "";
    }
    return it->second.networkId;
}

void PlayerManager::setNetworkInput(int playerId, float moveUp, float moveDown, float moveLeft, float moveRight,
                                    float actionWalk, float actionInteract, float actionThrow) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    auto it = players.find(playerId);
    if (it == players.end()) {
        return;
    }
    
    PlayerInput& player = it->second;
    player.networkInputValues[0] = moveUp;
    player.networkInputValues[1] = moveDown;
    player.networkInputValues[2] = moveLeft;
    player.networkInputValues[3] = moveRight;
    player.networkInputValues[4] = actionWalk;
    player.networkInputValues[5] = actionInteract;
    player.networkInputValues[6] = actionThrow;
    player.networkInputActive = true;
}

void PlayerManager::clearNetworkInput(int playerId) {
    std::lock_guard<std::mutex> lock(playersMutex);
    
    auto it = players.find(playerId);
    if (it == players.end()) {
        return;
    }
    
    PlayerInput& player = it->second;
    player.networkInputActive = false;
    for (size_t i = 0; i < static_cast<size_t>(GameAction::NUM_ACTIONS); ++i) {
        player.networkInputValues[i] = 0.0f;
    }
}

bool PlayerManager::hasNetworkInput(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end()) {
        return false;
    }
    return it->second.networkInputActive;
}

std::vector<int> PlayerManager::getAssignedPlayerIds() const {
    std::lock_guard<std::mutex> lock(playersMutex);
    std::vector<int> ids;
    ids.reserve(players.size());
    for (const auto& [id, _] : players) {
        ids.push_back(id);
    }
    return ids;
}

int PlayerManager::getPlayerIdByNetworkId(const std::string& networkId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    for (const auto& [playerId, player] : players) {
        if (!player.isLocal && player.networkId == networkId) {
            return playerId;
        }
    }
    return -1;
}

int PlayerManager::getPlayerIdByInputDevice(int inputSource) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    for (const auto& [playerId, player] : players) {
        if (player.isLocal) {
            for (int device : player.inputDevices) {
                if (device == inputSource) {
                    return playerId;
                }
            }
        }
    }
    return -1;
}

void PlayerManager::setPlayerConfigName(int playerId, const std::string& configName) {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it != players.end()) {
        it->second.configName = configName;
    }
}

std::string PlayerManager::getPlayerConfigName(int playerId) const {
    std::lock_guard<std::mutex> lock(playersMutex);
    auto it = players.find(playerId);
    if (it == players.end()) {
        return "";
    }
    return it->second.configName;
}

void PlayerManager::initializeDefaultAssignments() {
    // Use assignInputDevice to ensure each input device is only assigned to one player
    // This will automatically remove devices from other players if they're already assigned
    
    // Assign keyboard to player 1
    assignInputDevice(1, INPUT_SOURCE_KEYBOARD);
    
    // Assign first controller to player 1 (if available)
    if (inputManager.isInputSourceActive(0)) {
        assignInputDevice(1, 0);
    }
    
    // Assign additional controllers to additional players (controller 1 -> player 2, controller 2 -> player 3, etc.)
    // Start from player 2 since player 1 already has keyboard + controller 0
    int nextPlayerId = 2;
    for (int controllerIndex = 1; controllerIndex < 4; ++controllerIndex) {
        if (inputManager.isInputSourceActive(controllerIndex)) {
            // Check if this controller is already assigned
            int existingPlayerId = getPlayerIdByInputDevice(controllerIndex);
            
            if (existingPlayerId < 0) {
                // Controller not assigned yet - assign it to next available player ID
                // Find next available player ID that doesn't have a network assignment
                while (nextPlayerId <= 8) {
                    // Check if this player ID is already assigned to a network client
                    std::string networkId = getPlayerNetworkId(nextPlayerId);
                    if (networkId.empty()) {
                        // This player ID is available - assign controller to it
                        // assignInputDevice will ensure it's removed from any other player
                        assignInputDevice(nextPlayerId, controllerIndex);
                        nextPlayerId++;
                        break;
                    }
                    nextPlayerId++;
                }
            }
            // If controller is already assigned, leave it as is
        }
    }
}

void PlayerManager::clearAll() {
    std::lock_guard<std::mutex> lock(playersMutex);
    players.clear();
}

