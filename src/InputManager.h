#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <memory>

// Game action enum - represents in-game actions
enum class GameAction {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    ACTION_WALK,
    ACTION_INTERACT,
    ACTION_THROW,
    // Add more actions as needed
    NUM_ACTIONS
};

// Input source identifier
// -1 = Keyboard/Mouse
// 0, 1, 2, etc. = Controller index
constexpr int INPUT_SOURCE_KEYBOARD = -1;

// Forward declaration
class InputConfig;

class InputManager {
public:
    static InputManager& getInstance();
    
    // Initialize input system
    // configPath: path to input_config.json (optional, uses defaults if empty or file not found)
    void init(const std::string& configPath = "assets/input_config.json");
    
    // Update input states (call each frame)
    void update();
    
    // Cleanup
    void cleanup();
    
    // Get input value for a specific source and action (0.0 to 1.0)
    // Uses default config for the input source
    float getInputValue(int inputSource, GameAction action) const;
    
    // Get input value for a specific source and action with a named config (0.0 to 1.0)
    // If configName is empty, uses default config for the input source
    // If configName is provided, uses that named config to compute the value on-demand
    float getInputValue(int inputSource, GameAction action, const std::string& configName) const;
    
    // Load and cache a named configuration from a file
    // Returns true if loaded successfully, false otherwise
    bool loadNamedConfig(const std::string& configName, const std::string& configPath);
    
    // Get a named configuration (returns nullptr if not found)
    InputConfig* getNamedConfig(const std::string& configName);
    const InputConfig* getNamedConfig(const std::string& configName) const;
    
    // Check if an input source is active/connected
    bool isInputSourceActive(int inputSource) const;
    
    // Get number of connected controllers
    int getNumControllers() const;
    
    // Get input configuration (for reading or modifying)
    InputConfig& getConfig();
    const InputConfig& getConfig() const;
    
    // Get configuration for a specific input source
    InputConfig& getConfigForSource(int inputSource);
    const InputConfig& getConfigForSource(int inputSource) const;
    
    // Set configuration for a specific input source
    void setConfigForSource(int inputSource, const std::string& configPath);
    void setConfigForSource(int inputSource, std::unique_ptr<InputConfig> config);
    
    // Reload configuration from file
    bool reloadConfig(const std::string& configPath = "assets/input_config.json");
    
    // Hot-plug support
    void handleControllerAdded(int deviceIndex);
    void handleControllerRemoved(int instanceId);
    
    // Convert string to GameAction enum
    static GameAction stringToAction(const std::string& actionName);
    
    // Convert GameAction enum to string
    static std::string actionToString(GameAction action);
    
private:
    InputManager();
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    
    // Controller management
    void openController(int index);
    void closeController(int index);
    void updateKeyboardInput();
    void updateControllerInput(int controllerIndex);
    
    // Load gamecontrollerdb.txt
    void loadGameControllerDB(const std::string& path);
    
    // Input state storage
    // First index: input source (-1 for keyboard is mapped to 0, controllers are 1+)
    // Second index: GameAction enum
    std::array<std::array<float, static_cast<size_t>(GameAction::NUM_ACTIONS)>, 5> inputStates;
    
    // Raw device state storage (for on-demand computation with named configs)
    const Uint8* keyboardState; // Raw keyboard state (updated each frame)
    
    // Controller raw state (updated each frame)
    struct ControllerRawState {
        std::array<bool, SDL_CONTROLLER_BUTTON_MAX> buttons;
        std::array<Sint16, SDL_CONTROLLER_AXIS_MAX> axes;
    };
    std::array<ControllerRawState, 4> controllerRawStates;
    
    // SDL Controller handles (max 4 controllers)
    std::array<SDL_GameController*, 4> controllers;
    
    // Device index to controller slot mapping
    std::unordered_map<int, int> deviceToSlot;

    bool subsystemInitialized;
    bool cleanedUp;
    
    // Input configurations
    std::unique_ptr<InputConfig> config; // Default config
    std::unordered_map<int, std::unique_ptr<InputConfig>> sourceConfigs; // Per-source configs
    std::unordered_map<std::string, std::unique_ptr<InputConfig>> namedConfigs; // Named configs (per-object)
    
    // Helper to convert input source to array index
    int sourceToIndex(int inputSource) const;
    
    // Apply deadzone to analog input
    float applyDeadzone(float value, float deadzone) const;
    
    // Compute input value on-demand using a specific config
    float computeInputValue(int inputSource, GameAction action, const InputConfig& configToUse) const;
    
    // Update raw device state (called each frame)
    void updateRawDeviceState();
};

#endif // INPUT_MANAGER_H

