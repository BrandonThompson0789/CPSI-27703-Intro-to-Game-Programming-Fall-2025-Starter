#ifndef PLAYER_MANAGER_H
#define PLAYER_MANAGER_H

#include "InputManager.h"
#include <unordered_map>
#include <string>
#include <mutex>
#include <optional>

// PlayerManager manages the mapping between player IDs and their input sources
// Player IDs are abstract identifiers (0, 1, 2, etc.) that can be assigned to:
// - Local input devices (keyboard, controllers)
// - Network IDs (for remote players in multiplayer)

class PlayerManager {
public:
    static PlayerManager& getInstance();
    
    // Assign a local input device to a player ID
    // inputSource: -1 for keyboard/mouse, 0-3 for controllers
    // Returns true if assignment was successful
    bool assignInputDevice(int playerId, int inputSource);
    
    // Assign multiple input devices to a player ID (player can use any of them)
    bool assignInputDevices(int playerId, const std::vector<int>& inputSources);
    
    // Assign a network ID to a player ID (for remote players)
    // networkId: typically a string like "IP:PORT" or similar identifier
    bool assignNetworkId(int playerId, const std::string& networkId);
    
    // Remove assignment for a player ID
    void unassignPlayer(int playerId);
    
    // Get input value for a player (0.0 to 1.0)
    // Automatically handles both local and network input
    // If player has a config name set, it will be used; otherwise configName parameter is used
    float getInputValue(int playerId, GameAction action, const std::string& configName = "") const;
    
    // Set config name for a player (empty string = use default config)
    void setPlayerConfigName(int playerId, const std::string& configName);
    
    // Get config name for a player (returns empty if not set)
    std::string getPlayerConfigName(int playerId) const;
    
    // Initialize default player assignments (keyboard -> player 1, first controller -> player 1)
    // Should be called after InputManager is initialized
    void initializeDefaultAssignments();
    
    // Check if a player is assigned
    bool isPlayerAssigned(int playerId) const;
    
    // Check if a player is using local input (vs network input)
    bool isPlayerLocal(int playerId) const;
    
    // Get the input device(s) assigned to a player (returns empty if network player or not assigned)
    std::vector<int> getPlayerInputDevices(int playerId) const;
    
    // Get the network ID assigned to a player (returns empty if local player or not assigned)
    std::string getPlayerNetworkId(int playerId) const;
    
    // Set network input values for a player (used by HostManager/ClientManager)
    void setNetworkInput(int playerId, float moveUp, float moveDown, float moveLeft, float moveRight,
                        float actionWalk, float actionInteract, float actionThrow);
    
    // Clear network input for a player (revert to local input)
    void clearNetworkInput(int playerId);
    
    // Check if network input is active for a player
    bool hasNetworkInput(int playerId) const;
    
    // Get all assigned player IDs
    std::vector<int> getAssignedPlayerIds() const;
    
    // Find player ID by network ID (returns -1 if not found)
    int getPlayerIdByNetworkId(const std::string& networkId) const;
    
    // Find player ID by input device (returns -1 if not found)
    int getPlayerIdByInputDevice(int inputSource) const;
    
    // Clear all assignments
    void clearAll();

private:
    PlayerManager();
    ~PlayerManager() = default;
    PlayerManager(const PlayerManager&) = delete;
    PlayerManager& operator=(const PlayerManager&) = delete;
    
    struct PlayerInput {
        std::vector<int> inputDevices;  // Local input devices (empty if network player)
        std::string networkId;          // Network ID (empty if local player)
        bool isLocal;                    // True if using local input, false if network
        std::string configName;          // Config name for this player (empty = use default)
        
        // Network input storage (when network input is active)
        bool networkInputActive;
        float networkInputValues[7]; // MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT, ACTION_WALK, ACTION_INTERACT, ACTION_THROW
    };
    
    std::unordered_map<int, PlayerInput> players;
    mutable std::mutex playersMutex;
    
    InputManager& inputManager;
};

#endif // PLAYER_MANAGER_H

