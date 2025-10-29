#ifndef INPUT_CONFIG_H
#define INPUT_CONFIG_H

#include <SDL.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "InputManager.h"

// Keyboard key mapping
struct KeyboardMapping {
    std::vector<SDL_Scancode> keys;
};

// Controller button mapping
struct ControllerButtonMapping {
    std::vector<SDL_GameControllerButton> buttons;
};

// Controller axis mapping
struct ControllerAxisMapping {
    SDL_GameControllerAxis axis;
    bool positive;  // true = positive direction, false = negative direction
    bool fullRange; // true = use full 0.0-1.0 range (triggers), false = use -1.0 to 1.0 (sticks)
};

// Input mapping type
enum class InputMappingType {
    BUTTON,
    AXIS
};

// Controller action mapping (can be button or axis)
struct ControllerMapping {
    InputMappingType type;
    ControllerButtonMapping buttonMapping;
    ControllerAxisMapping axisMapping;
};

class InputConfig {
public:
    InputConfig();
    
    // Load configuration from JSON file
    bool loadFromFile(const std::string& filename);
    
    // Save current configuration to JSON file
    bool saveToFile(const std::string& filename) const;
    
    // Get keyboard mappings for an action
    const KeyboardMapping& getKeyboardMapping(GameAction action) const;
    
    // Get controller mappings for an action
    const ControllerMapping& getControllerMapping(GameAction action) const;
    
    // Set keyboard mapping for an action
    void setKeyboardMapping(GameAction action, const std::vector<SDL_Scancode>& keys);
    
    // Set controller button mapping for an action
    void setControllerButtonMapping(GameAction action, const std::vector<SDL_GameControllerButton>& buttons);
    
    // Set controller axis mapping for an action
    void setControllerAxisMapping(GameAction action, SDL_GameControllerAxis axis, bool positive, bool fullRange = false);
    
    // Get deadzone value
    float getDeadzone() const { return deadzone; }
    
    // Set deadzone value
    void setDeadzone(float value) { deadzone = value; }
    
    // Get D-pad as axis setting
    bool getDPadAsAxis() const { return dpadAsAxis; }
    
    // Load default configuration
    void loadDefaults();
    
private:
    // Keyboard mappings for each action
    std::unordered_map<GameAction, KeyboardMapping> keyboardMappings;
    
    // Controller mappings for each action
    std::unordered_map<GameAction, ControllerMapping> controllerMappings;
    
    // Settings
    float deadzone;
    bool dpadAsAxis;
    
    // Helper functions for JSON parsing
    SDL_Scancode stringToScancode(const std::string& keyName) const;
    std::string scancodeToString(SDL_Scancode scancode) const;
    SDL_GameControllerButton stringToButton(const std::string& buttonName) const;
    std::string buttonToString(SDL_GameControllerButton button) const;
    SDL_GameControllerAxis stringToAxis(const std::string& axisName) const;
    std::string axisToString(SDL_GameControllerAxis axis) const;
    
    // Parse JSON sections
    void parseKeyboardMappings(const nlohmann::json& json);
    void parseControllerMappings(const nlohmann::json& json);
    void parseSettings(const nlohmann::json& json);
};

#endif // INPUT_CONFIG_H

