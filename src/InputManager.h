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
    float getInputValue(int inputSource, GameAction action) const;
    
    // Check if an input source is active/connected
    bool isInputSourceActive(int inputSource) const;
    
    // Get number of connected controllers
    int getNumControllers() const;
    
    // Get input configuration (for reading or modifying)
    InputConfig& getConfig();
    const InputConfig& getConfig() const;
    
    // Reload configuration from file
    bool reloadConfig(const std::string& configPath = "assets/input_config.json");
    
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
    
    // SDL Controller handles (max 4 controllers)
    std::array<SDL_GameController*, 4> controllers;
    
    // Input configuration
    std::unique_ptr<InputConfig> config;
    
    // Helper to convert input source to array index
    int sourceToIndex(int inputSource) const;
    
    // Apply deadzone to analog input
    float applyDeadzone(float value, float deadzone) const;
};

#endif // INPUT_MANAGER_H

