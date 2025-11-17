#ifndef INPUT_COMPONENT_H
#define INPUT_COMPONENT_H

#include "Component.h"
#include "../PlayerManager.h"
#include <nlohmann/json.hpp>

class InputComponent : public Component {
public:
    // Constructor - specify which player ID this component uses
    // playerId: player ID that should be assigned to an input device or network ID via PlayerManager
    // Default is player 1 (keyboard is assigned to player 1 by default)
    InputComponent(Object& parent, int playerId = 1);
    
    // Constructor from JSON
    // Expects "playerId" field (defaults to 1 if not specified)
    // Optional "configName" field to set player's input config
    InputComponent(Object& parent, const nlohmann::json& data);
    
    virtual ~InputComponent() = default;
    
    void update(float deltaTime) override;
    void draw() override {} // Input component doesn't draw
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "InputComponent"; }
    
    // Get input value for a specific action (0.0 to 1.0)
    float getInput(GameAction action) const;
    
    // Convenience methods for common actions
    float getMoveUp() const { return getInput(GameAction::MOVE_UP); }
    float getMoveDown() const { return getInput(GameAction::MOVE_DOWN); }
    float getMoveLeft() const { return getInput(GameAction::MOVE_LEFT); }
    float getMoveRight() const { return getInput(GameAction::MOVE_RIGHT); }
    float getActionWalk() const { return getInput(GameAction::ACTION_WALK); }
    float getActionInteract() const { return getInput(GameAction::ACTION_INTERACT); }
    float getActionThrow() const { return getInput(GameAction::ACTION_THROW); }
    
    // Check if button is pressed (value > 0.5)
    bool isPressed(GameAction action) const;
    
    // Get player ID
    int getPlayerId() const { return playerId; }
    
    // Set player ID
    void setPlayerId(int id) { playerId = id; }
    
    // Check if player is active (has input assigned and is providing input)
    bool isActive() const;
    
    // Get config name (from PlayerManager)
    std::string getConfigName() const;
    
    // Set config name (empty string = use default config)
    // This sets the config name in PlayerManager for this player
    void setConfigName(const std::string& name);
    
private:
    int playerId;
    PlayerManager& playerManager;
};

#endif // INPUT_COMPONENT_H

