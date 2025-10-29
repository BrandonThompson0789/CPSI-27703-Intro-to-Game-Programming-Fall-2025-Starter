#include "InputManager.h"
#include "InputConfig.h"
#include <iostream>
#include <cmath>

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

InputManager::InputManager() {
    // Initialize input states to 0
    for (auto& source : inputStates) {
        source.fill(0.0f);
    }
    
    // Initialize controller handles to nullptr
    controllers.fill(nullptr);
    
    // Create config with defaults
    config = std::make_unique<InputConfig>();
}

InputManager::~InputManager() {
    cleanup();
}

void InputManager::init(const std::string& configPath) {
    // Initialize SDL game controller subsystem
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "Failed to initialize SDL GameController subsystem: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Load gamecontrollerdb.txt
    loadGameControllerDB("assets/gamecontrollerdb.txt");
    
    // Load input configuration
    if (!configPath.empty()) {
        if (!config->loadFromFile(configPath)) {
            std::cout << "Using default input configuration" << std::endl;
        }
    } else {
        std::cout << "Using default input configuration" << std::endl;
    }
    
    // Open all available controllers
    int numJoysticks = SDL_NumJoysticks();
    std::cout << "Found " << numJoysticks << " joystick(s)" << std::endl;
    
    for (int i = 0; i < numJoysticks && i < 4; ++i) {
        if (SDL_IsGameController(i)) {
            openController(i);
        }
    }
    
    std::cout << "InputManager initialized" << std::endl;
}

void InputManager::loadGameControllerDB(const std::string& path) {
    int result = SDL_GameControllerAddMappingsFromFile(path.c_str());
    if (result == -1) {
        std::cerr << "Warning: Could not load gamecontrollerdb.txt from " << path << std::endl;
        std::cerr << "SDL Error: " << SDL_GetError() << std::endl;
    } else {
        std::cout << "Loaded " << result << " controller mappings from " << path << std::endl;
    }
}

void InputManager::update() {
    // Clear all input states
    for (auto& source : inputStates) {
        source.fill(0.0f);
    }
    
    // Update keyboard input
    updateKeyboardInput();
    
    // Update each connected controller
    for (int i = 0; i < 4; ++i) {
        if (controllers[i] != nullptr) {
            updateControllerInput(i);
        }
    }
}

void InputManager::updateKeyboardInput() {
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    int keyboardIndex = sourceToIndex(INPUT_SOURCE_KEYBOARD);
    
    // Get configuration for keyboard (per-source or default)
    const InputConfig& keyboardConfig = getConfigForSource(INPUT_SOURCE_KEYBOARD);
    
    // Iterate through all game actions and check configured keys
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        const KeyboardMapping& mapping = keyboardConfig.getKeyboardMapping(action);
        
        // Check if any of the mapped keys are pressed
        for (SDL_Scancode key : mapping.keys) {
            if (state[key]) {
                inputStates[keyboardIndex][i] = 1.0f;
                break; // Key is pressed, no need to check others
            }
        }
    }
}

void InputManager::updateControllerInput(int controllerIndex) {
    SDL_GameController* controller = controllers[controllerIndex];
    if (controller == nullptr || !SDL_GameControllerGetAttached(controller)) {
        return;
    }
    
    int stateIndex = sourceToIndex(controllerIndex);
    
    // Get configuration for this controller (per-source or default)
    const InputConfig& controllerConfig = getConfigForSource(controllerIndex);
    float deadzone = controllerConfig.getDeadzone();
    
    // Iterate through all game actions and check configured mappings
    for (int i = 0; i < static_cast<int>(GameAction::NUM_ACTIONS); ++i) {
        GameAction action = static_cast<GameAction>(i);
        const ControllerMapping& mapping = controllerConfig.getControllerMapping(action);
        
        if (mapping.type == InputMappingType::BUTTON) {
            // Button mapping - check if any mapped button is pressed
            for (SDL_GameControllerButton button : mapping.buttonMapping.buttons) {
                if (SDL_GameControllerGetButton(controller, button)) {
                    inputStates[stateIndex][i] = 1.0f;
                    break;
                }
            }
            
        } else { // AXIS
            // Axis mapping
            const ControllerAxisMapping& axisMapping = mapping.axisMapping;
            float rawValue = SDL_GameControllerGetAxis(controller, axisMapping.axis) / 32767.0f;
            
            // Apply deadzone
            float processedValue = applyDeadzone(rawValue, deadzone);
            
            // Extract the desired direction
            if (axisMapping.fullRange) {
                // Full range (0.0 to 1.0) - for triggers
                inputStates[stateIndex][i] = std::max(0.0f, processedValue);
            } else {
                // Directional - extract positive or negative direction
                if (axisMapping.positive) {
                    // Want positive direction
                    if (processedValue > 0.0f) {
                        inputStates[stateIndex][i] = processedValue;
                    }
                } else {
                    // Want negative direction
                    if (processedValue < 0.0f) {
                        inputStates[stateIndex][i] = std::abs(processedValue);
                    }
                }
            }
        }
    }
    
    // D-pad buttons (if configured to act as digital buttons, they override axis input)
    if (controllerConfig.getDPadAsAxis()) {
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            inputStates[stateIndex][static_cast<size_t>(GameAction::MOVE_UP)] = 1.0f;
        }
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
            inputStates[stateIndex][static_cast<size_t>(GameAction::MOVE_DOWN)] = 1.0f;
        }
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
            inputStates[stateIndex][static_cast<size_t>(GameAction::MOVE_LEFT)] = 1.0f;
        }
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
            inputStates[stateIndex][static_cast<size_t>(GameAction::MOVE_RIGHT)] = 1.0f;
        }
    }
}

float InputManager::applyDeadzone(float value, float deadzone) const {
    if (std::abs(value) < deadzone) {
        return 0.0f;
    }
    
    // Rescale to 0.0-1.0 outside deadzone
    float sign = (value > 0) ? 1.0f : -1.0f;
    float absValue = std::abs(value);
    return sign * ((absValue - deadzone) / (1.0f - deadzone));
}

void InputManager::openController(int index) {
    if (index < 0 || index >= 4) {
        return;
    }
    
    SDL_GameController* controller = SDL_GameControllerOpen(index);
    if (controller == nullptr) {
        std::cerr << "Could not open controller " << index << ": " << SDL_GetError() << std::endl;
        return;
    }
    
    controllers[index] = controller;
    const char* name = SDL_GameControllerName(controller);
    std::cout << "Opened controller " << index << ": " << (name ? name : "Unknown") << std::endl;
}

void InputManager::closeController(int index) {
    if (index < 0 || index >= 4 || controllers[index] == nullptr) {
        return;
    }
    
    SDL_GameControllerClose(controllers[index]);
    controllers[index] = nullptr;
}

void InputManager::cleanup() {
    // Close all controllers
    for (int i = 0; i < 4; ++i) {
        closeController(i);
    }
    
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

float InputManager::getInputValue(int inputSource, GameAction action) const {
    int index = sourceToIndex(inputSource);
    if (index < 0 || index >= static_cast<int>(inputStates.size())) {
        return 0.0f;
    }
    
    size_t actionIndex = static_cast<size_t>(action);
    if (actionIndex >= static_cast<size_t>(GameAction::NUM_ACTIONS)) {
        return 0.0f;
    }
    
    return inputStates[index][actionIndex];
}

bool InputManager::isInputSourceActive(int inputSource) const {
    if (inputSource == INPUT_SOURCE_KEYBOARD) {
        return true; // Keyboard is always active
    }
    
    if (inputSource < 0 || inputSource >= 4) {
        return false;
    }
    
    return controllers[inputSource] != nullptr && 
           SDL_GameControllerGetAttached(controllers[inputSource]);
}

int InputManager::getNumControllers() const {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (controllers[i] != nullptr && SDL_GameControllerGetAttached(controllers[i])) {
            count++;
        }
    }
    return count;
}

int InputManager::sourceToIndex(int inputSource) const {
    // Keyboard (-1) maps to index 0
    // Controller 0 maps to index 1
    // Controller 1 maps to index 2, etc.
    return inputSource + 1;
}

InputConfig& InputManager::getConfig() {
    return *config;
}

const InputConfig& InputManager::getConfig() const {
    return *config;
}

bool InputManager::reloadConfig(const std::string& configPath) {
    return config->loadFromFile(configPath);
}

InputConfig& InputManager::getConfigForSource(int inputSource) {
    // Check if there's a per-source config
    auto it = sourceConfigs.find(inputSource);
    if (it != sourceConfigs.end() && it->second) {
        return *it->second;
    }
    // Return default config
    return *config;
}

const InputConfig& InputManager::getConfigForSource(int inputSource) const {
    // Check if there's a per-source config
    auto it = sourceConfigs.find(inputSource);
    if (it != sourceConfigs.end() && it->second) {
        return *it->second;
    }
    // Return default config
    return *config;
}

void InputManager::setConfigForSource(int inputSource, const std::string& configPath) {
    auto sourceConfig = std::make_unique<InputConfig>();
    if (sourceConfig->loadFromFile(configPath)) {
        sourceConfigs[inputSource] = std::move(sourceConfig);
        std::cout << "Loaded configuration for input source " << inputSource << " from " << configPath << std::endl;
    } else {
        std::cerr << "Failed to load configuration for input source " << inputSource << std::endl;
    }
}

void InputManager::setConfigForSource(int inputSource, std::unique_ptr<InputConfig> cfg) {
    sourceConfigs[inputSource] = std::move(cfg);
    std::cout << "Set custom configuration for input source " << inputSource << std::endl;
}

void InputManager::handleControllerAdded(int deviceIndex) {
    // Find an empty slot for the new controller
    for (int slot = 0; slot < 4; ++slot) {
        if (controllers[slot] == nullptr) {
            if (SDL_IsGameController(deviceIndex)) {
                SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
                if (controller) {
                    controllers[slot] = controller;
                    deviceToSlot[deviceIndex] = slot;
                    
                    const char* name = SDL_GameControllerName(controller);
                    std::cout << "Controller connected in slot " << slot << ": " 
                              << (name ? name : "Unknown") << std::endl;
                    return;
                }
            }
        }
    }
    
    std::cout << "No available slot for new controller (max 4)" << std::endl;
}

void InputManager::handleControllerRemoved(int instanceId) {
    // Find which slot this controller was in
    for (int slot = 0; slot < 4; ++slot) {
        if (controllers[slot] != nullptr) {
            SDL_JoystickID joyId = SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(controllers[slot])
            );
            
            if (joyId == instanceId) {
                SDL_GameControllerClose(controllers[slot]);
                controllers[slot] = nullptr;
                
                // Remove from device mapping
                for (auto it = deviceToSlot.begin(); it != deviceToSlot.end(); ++it) {
                    if (it->second == slot) {
                        deviceToSlot.erase(it);
                        break;
                    }
                }
                
                std::cout << "Controller disconnected from slot " << slot << std::endl;
                return;
            }
        }
    }
}

GameAction InputManager::stringToAction(const std::string& actionName) {
    if (actionName == "move_up") return GameAction::MOVE_UP;
    if (actionName == "move_down") return GameAction::MOVE_DOWN;
    if (actionName == "move_left") return GameAction::MOVE_LEFT;
    if (actionName == "move_right") return GameAction::MOVE_RIGHT;
    if (actionName == "action_walk") return GameAction::ACTION_WALK;
    if (actionName == "action_interact") return GameAction::ACTION_INTERACT;
    if (actionName == "action_throw") return GameAction::ACTION_THROW;
    
    return GameAction::NUM_ACTIONS; // Invalid action
}

std::string InputManager::actionToString(GameAction action) {
    switch (action) {
        case GameAction::MOVE_UP: return "move_up";
        case GameAction::MOVE_DOWN: return "move_down";
        case GameAction::MOVE_LEFT: return "move_left";
        case GameAction::MOVE_RIGHT: return "move_right";
        case GameAction::ACTION_WALK: return "action_walk";
        case GameAction::ACTION_INTERACT: return "action_interact";
        case GameAction::ACTION_THROW: return "action_throw";
        default: return "unknown";
    }
}

