#include "InputConfig.h"
#include <fstream>
#include <iostream>

InputConfig::InputConfig() 
    : deadzone(0.15f)
    , dpadAsAxis(true)
{
    loadDefaults();
}

bool InputConfig::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open input config file: " << filename << std::endl;
        std::cerr << "Using default configuration" << std::endl;
        loadDefaults();
        return false;
    }
    
    try {
        nlohmann::json json;
        file >> json;
        file.close();
        
        // Parse each section
        if (json.contains("keyboard")) {
            parseKeyboardMappings(json["keyboard"]);
        }
        
        if (json.contains("controller")) {
            parseControllerMappings(json["controller"]);
        }
        
        if (json.contains("settings")) {
            parseSettings(json["settings"]);
        }
        
        std::cout << "Input configuration loaded from " << filename << std::endl;
        return true;
        
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing input config: " << e.what() << std::endl;
        std::cerr << "Using default configuration" << std::endl;
        loadDefaults();
        return false;
    }
}

bool InputConfig::saveToFile(const std::string& filename) const {
    nlohmann::json json;
    
    // Save keyboard mappings
    nlohmann::json keyboardJson;
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        std::string actionName = InputManager::actionToString(action);
        
        auto it = keyboardMappings.find(action);
        if (it != keyboardMappings.end()) {
            nlohmann::json keysArray;
            for (SDL_Scancode key : it->second.keys) {
                keysArray.push_back(scancodeToString(key));
            }
            keyboardJson[actionName] = keysArray;
        }
    }
    json["keyboard"] = keyboardJson;
    
    // Save controller mappings
    nlohmann::json controllerJson;
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        std::string actionName = InputManager::actionToString(action);
        
        auto it = controllerMappings.find(action);
        if (it != controllerMappings.end()) {
            const ControllerMapping& mapping = it->second;
            
            if (mapping.type == InputMappingType::BUTTON) {
                nlohmann::json buttonsArray;
                for (SDL_GameControllerButton button : mapping.buttonMapping.buttons) {
                    buttonsArray.push_back(buttonToString(button));
                }
                
                nlohmann::json mappingJson;
                mappingJson["type"] = "button";
                mappingJson["buttons"] = buttonsArray;
                controllerJson[actionName] = mappingJson;
                
            } else { // AXIS
                nlohmann::json mappingJson;
                mappingJson["type"] = "axis";
                mappingJson["axis"] = axisToString(mapping.axisMapping.axis);
                mappingJson["direction"] = mapping.axisMapping.positive ? "positive" : "negative";
                if (mapping.axisMapping.fullRange) {
                    mappingJson["range"] = "full";
                }
                controllerJson[actionName] = mappingJson;
            }
        }
    }
    json["controller"] = controllerJson;
    
    // Save settings
    nlohmann::json settingsJson;
    settingsJson["deadzone"] = deadzone;
    settingsJson["dpad_as_axis"] = dpadAsAxis;
    json["settings"] = settingsJson;
    
    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write input config to " << filename << std::endl;
        return false;
    }
    
    file << json.dump(2); // Pretty print with 2-space indent
    file.close();
    
    std::cout << "Input configuration saved to " << filename << std::endl;
    return true;
}

void InputConfig::loadDefaults() {
    // Default keyboard mappings
    setKeyboardMapping(GameAction::MOVE_UP, {SDL_SCANCODE_W, SDL_SCANCODE_UP});
    setKeyboardMapping(GameAction::MOVE_DOWN, {SDL_SCANCODE_S, SDL_SCANCODE_DOWN});
    setKeyboardMapping(GameAction::MOVE_LEFT, {SDL_SCANCODE_A, SDL_SCANCODE_LEFT});
    setKeyboardMapping(GameAction::MOVE_RIGHT, {SDL_SCANCODE_D, SDL_SCANCODE_RIGHT});
    setKeyboardMapping(GameAction::ACTION_WALK, {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT});
    setKeyboardMapping(GameAction::ACTION_INTERACT, {SDL_SCANCODE_SPACE, SDL_SCANCODE_E});
    setKeyboardMapping(GameAction::ACTION_THROW, {SDL_SCANCODE_F, SDL_SCANCODE_Q});
    setKeyboardMapping(GameAction::ACTION_PAUSE, {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN});
    
    // Default controller mappings
    setControllerAxisMapping(GameAction::MOVE_UP, SDL_CONTROLLER_AXIS_LEFTY, false); // Negative Y
    setControllerAxisMapping(GameAction::MOVE_DOWN, SDL_CONTROLLER_AXIS_LEFTY, true); // Positive Y
    setControllerAxisMapping(GameAction::MOVE_LEFT, SDL_CONTROLLER_AXIS_LEFTX, false); // Negative X
    setControllerAxisMapping(GameAction::MOVE_RIGHT, SDL_CONTROLLER_AXIS_LEFTX, true); // Positive X
    setControllerAxisMapping(GameAction::ACTION_WALK, SDL_CONTROLLER_AXIS_TRIGGERLEFT, true, true); // Trigger (full range)
    setControllerButtonMapping(GameAction::ACTION_INTERACT, {SDL_CONTROLLER_BUTTON_A});
    setControllerButtonMapping(GameAction::ACTION_THROW, {SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_B});
    setControllerButtonMapping(GameAction::ACTION_PAUSE, {SDL_CONTROLLER_BUTTON_START});
    
    deadzone = 0.15f;
    dpadAsAxis = true;
}

const KeyboardMapping& InputConfig::getKeyboardMapping(GameAction action) const {
    static KeyboardMapping empty;
    auto it = keyboardMappings.find(action);
    if (it != keyboardMappings.end()) {
        return it->second;
    }
    return empty;
}

const ControllerMapping& InputConfig::getControllerMapping(GameAction action) const {
    static ControllerMapping empty;
    auto it = controllerMappings.find(action);
    if (it != controllerMappings.end()) {
        return it->second;
    }
    return empty;
}

void InputConfig::setKeyboardMapping(GameAction action, const std::vector<SDL_Scancode>& keys) {
    KeyboardMapping mapping;
    mapping.keys = keys;
    keyboardMappings[action] = mapping;
}

void InputConfig::setControllerButtonMapping(GameAction action, const std::vector<SDL_GameControllerButton>& buttons) {
    ControllerMapping mapping;
    mapping.type = InputMappingType::BUTTON;
    mapping.buttonMapping.buttons = buttons;
    controllerMappings[action] = mapping;
}

void InputConfig::setControllerAxisMapping(GameAction action, SDL_GameControllerAxis axis, bool positive, bool fullRange) {
    ControllerMapping mapping;
    mapping.type = InputMappingType::AXIS;
    mapping.axisMapping.axis = axis;
    mapping.axisMapping.positive = positive;
    mapping.axisMapping.fullRange = fullRange;
    controllerMappings[action] = mapping;
}

void InputConfig::parseKeyboardMappings(const nlohmann::json& json) {
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        std::string actionName = InputManager::actionToString(action);
        
        if (json.contains(actionName)) {
            std::vector<SDL_Scancode> keys;
            
            if (json[actionName].is_array()) {
                for (const auto& keyName : json[actionName]) {
                    SDL_Scancode scancode = stringToScancode(keyName.get<std::string>());
                    if (scancode != SDL_SCANCODE_UNKNOWN) {
                        keys.push_back(scancode);
                    }
                }
            } else if (json[actionName].is_string()) {
                SDL_Scancode scancode = stringToScancode(json[actionName].get<std::string>());
                if (scancode != SDL_SCANCODE_UNKNOWN) {
                    keys.push_back(scancode);
                }
            }
            
            if (!keys.empty()) {
                setKeyboardMapping(action, keys);
            }
        }
    }
}

void InputConfig::parseControllerMappings(const nlohmann::json& json) {
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        std::string actionName = InputManager::actionToString(action);
        
        if (json.contains(actionName)) {
            const auto& mapping = json[actionName];
            
            if (mapping.contains("type")) {
                std::string type = mapping["type"].get<std::string>();
                
                if (type == "button" && mapping.contains("buttons")) {
                    std::vector<SDL_GameControllerButton> buttons;
                    
                    if (mapping["buttons"].is_array()) {
                        for (const auto& buttonName : mapping["buttons"]) {
                            SDL_GameControllerButton button = stringToButton(buttonName.get<std::string>());
                            if (button != SDL_CONTROLLER_BUTTON_INVALID) {
                                buttons.push_back(button);
                            }
                        }
                    } else if (mapping["buttons"].is_string()) {
                        SDL_GameControllerButton button = stringToButton(mapping["buttons"].get<std::string>());
                        if (button != SDL_CONTROLLER_BUTTON_INVALID) {
                            buttons.push_back(button);
                        }
                    }
                    
                    if (!buttons.empty()) {
                        setControllerButtonMapping(action, buttons);
                    }
                    
                } else if (type == "axis" && mapping.contains("axis")) {
                    SDL_GameControllerAxis axis = stringToAxis(mapping["axis"].get<std::string>());
                    bool positive = true;
                    bool fullRange = false;
                    
                    if (mapping.contains("direction")) {
                        std::string direction = mapping["direction"].get<std::string>();
                        positive = (direction == "positive");
                    }
                    
                    if (mapping.contains("range")) {
                        std::string range = mapping["range"].get<std::string>();
                        fullRange = (range == "full");
                    }
                    
                    if (axis != SDL_CONTROLLER_AXIS_INVALID) {
                        setControllerAxisMapping(action, axis, positive, fullRange);
                    }
                }
            }
        }
    }
}

void InputConfig::parseSettings(const nlohmann::json& json) {
    if (json.contains("deadzone")) {
        deadzone = json["deadzone"].get<float>();
    }
    
    if (json.contains("dpad_as_axis")) {
        dpadAsAxis = json["dpad_as_axis"].get<bool>();
    }
}

// String to SDL enum conversions
SDL_Scancode InputConfig::stringToScancode(const std::string& keyName) const {
    // Common keys
    if (keyName == "W") return SDL_SCANCODE_W;
    if (keyName == "A") return SDL_SCANCODE_A;
    if (keyName == "S") return SDL_SCANCODE_S;
    if (keyName == "D") return SDL_SCANCODE_D;
    if (keyName == "E") return SDL_SCANCODE_E;
    if (keyName == "Q") return SDL_SCANCODE_Q;
    if (keyName == "F") return SDL_SCANCODE_F;
    if (keyName == "R") return SDL_SCANCODE_R;
    if (keyName == "Space") return SDL_SCANCODE_SPACE;
    if (keyName == "LShift") return SDL_SCANCODE_LSHIFT;
    if (keyName == "RShift") return SDL_SCANCODE_RSHIFT;
    if (keyName == "LCtrl") return SDL_SCANCODE_LCTRL;
    if (keyName == "RCtrl") return SDL_SCANCODE_RCTRL;
    if (keyName == "LAlt") return SDL_SCANCODE_LALT;
    if (keyName == "RAlt") return SDL_SCANCODE_RALT;
    
    // Arrow keys
    if (keyName == "Up") return SDL_SCANCODE_UP;
    if (keyName == "Down") return SDL_SCANCODE_DOWN;
    if (keyName == "Left") return SDL_SCANCODE_LEFT;
    if (keyName == "Right") return SDL_SCANCODE_RIGHT;
    
    // Numbers
    if (keyName == "0") return SDL_SCANCODE_0;
    if (keyName == "1") return SDL_SCANCODE_1;
    if (keyName == "2") return SDL_SCANCODE_2;
    if (keyName == "3") return SDL_SCANCODE_3;
    if (keyName == "4") return SDL_SCANCODE_4;
    if (keyName == "5") return SDL_SCANCODE_5;
    if (keyName == "6") return SDL_SCANCODE_6;
    if (keyName == "7") return SDL_SCANCODE_7;
    if (keyName == "8") return SDL_SCANCODE_8;
    if (keyName == "9") return SDL_SCANCODE_9;
    
    // Other letters
    if (keyName == "B") return SDL_SCANCODE_B;
    if (keyName == "C") return SDL_SCANCODE_C;
    if (keyName == "G") return SDL_SCANCODE_G;
    if (keyName == "H") return SDL_SCANCODE_H;
    if (keyName == "I") return SDL_SCANCODE_I;
    if (keyName == "J") return SDL_SCANCODE_J;
    if (keyName == "K") return SDL_SCANCODE_K;
    if (keyName == "L") return SDL_SCANCODE_L;
    if (keyName == "M") return SDL_SCANCODE_M;
    if (keyName == "N") return SDL_SCANCODE_N;
    if (keyName == "O") return SDL_SCANCODE_O;
    if (keyName == "P") return SDL_SCANCODE_P;
    if (keyName == "T") return SDL_SCANCODE_T;
    if (keyName == "U") return SDL_SCANCODE_U;
    if (keyName == "V") return SDL_SCANCODE_V;
    if (keyName == "X") return SDL_SCANCODE_X;
    if (keyName == "Y") return SDL_SCANCODE_Y;
    if (keyName == "Z") return SDL_SCANCODE_Z;
    
    // Special keys
    if (keyName == "Tab") return SDL_SCANCODE_TAB;
    if (keyName == "Enter") return SDL_SCANCODE_RETURN;
    if (keyName == "Escape") return SDL_SCANCODE_ESCAPE;
    if (keyName == "Backspace") return SDL_SCANCODE_BACKSPACE;
    
    return SDL_SCANCODE_UNKNOWN;
}

std::string InputConfig::scancodeToString(SDL_Scancode scancode) const {
    switch (scancode) {
        case SDL_SCANCODE_W: return "W";
        case SDL_SCANCODE_A: return "A";
        case SDL_SCANCODE_S: return "S";
        case SDL_SCANCODE_D: return "D";
        case SDL_SCANCODE_E: return "E";
        case SDL_SCANCODE_Q: return "Q";
        case SDL_SCANCODE_F: return "F";
        case SDL_SCANCODE_R: return "R";
        case SDL_SCANCODE_SPACE: return "Space";
        case SDL_SCANCODE_LSHIFT: return "LShift";
        case SDL_SCANCODE_RSHIFT: return "RShift";
        case SDL_SCANCODE_LCTRL: return "LCtrl";
        case SDL_SCANCODE_RCTRL: return "RCtrl";
        case SDL_SCANCODE_UP: return "Up";
        case SDL_SCANCODE_DOWN: return "Down";
        case SDL_SCANCODE_LEFT: return "Left";
        case SDL_SCANCODE_RIGHT: return "Right";
        default: return "Unknown";
    }
}

SDL_GameControllerButton InputConfig::stringToButton(const std::string& buttonName) const {
    if (buttonName == "A") return SDL_CONTROLLER_BUTTON_A;
    if (buttonName == "B") return SDL_CONTROLLER_BUTTON_B;
    if (buttonName == "X") return SDL_CONTROLLER_BUTTON_X;
    if (buttonName == "Y") return SDL_CONTROLLER_BUTTON_Y;
    if (buttonName == "Back") return SDL_CONTROLLER_BUTTON_BACK;
    if (buttonName == "Guide") return SDL_CONTROLLER_BUTTON_GUIDE;
    if (buttonName == "Start") return SDL_CONTROLLER_BUTTON_START;
    if (buttonName == "LeftStick") return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (buttonName == "RightStick") return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    if (buttonName == "LeftShoulder") return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (buttonName == "RightShoulder") return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (buttonName == "DPadUp") return SDL_CONTROLLER_BUTTON_DPAD_UP;
    if (buttonName == "DPadDown") return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    if (buttonName == "DPadLeft") return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    if (buttonName == "DPadRight") return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    
    return SDL_CONTROLLER_BUTTON_INVALID;
}

std::string InputConfig::buttonToString(SDL_GameControllerButton button) const {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_A: return "A";
        case SDL_CONTROLLER_BUTTON_B: return "B";
        case SDL_CONTROLLER_BUTTON_X: return "X";
        case SDL_CONTROLLER_BUTTON_Y: return "Y";
        case SDL_CONTROLLER_BUTTON_BACK: return "Back";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "Guide";
        case SDL_CONTROLLER_BUTTON_START: return "Start";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LeftStick";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RightStick";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LeftShoulder";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RightShoulder";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "DPadUp";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "DPadDown";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "DPadLeft";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "DPadRight";
        default: return "Unknown";
    }
}

SDL_GameControllerAxis InputConfig::stringToAxis(const std::string& axisName) const {
    if (axisName == "LeftX") return SDL_CONTROLLER_AXIS_LEFTX;
    if (axisName == "LeftY") return SDL_CONTROLLER_AXIS_LEFTY;
    if (axisName == "RightX") return SDL_CONTROLLER_AXIS_RIGHTX;
    if (axisName == "RightY") return SDL_CONTROLLER_AXIS_RIGHTY;
    if (axisName == "TriggerLeft") return SDL_CONTROLLER_AXIS_TRIGGERLEFT;
    if (axisName == "TriggerRight") return SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
    
    return SDL_CONTROLLER_AXIS_INVALID;
}

std::string InputConfig::axisToString(SDL_GameControllerAxis axis) const {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return "LeftX";
        case SDL_CONTROLLER_AXIS_LEFTY: return "LeftY";
        case SDL_CONTROLLER_AXIS_RIGHTX: return "RightX";
        case SDL_CONTROLLER_AXIS_RIGHTY: return "RightY";
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "TriggerLeft";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "TriggerRight";
        default: return "Unknown";
    }
}

