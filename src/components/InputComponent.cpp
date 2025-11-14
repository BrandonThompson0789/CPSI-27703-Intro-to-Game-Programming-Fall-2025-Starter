#include "InputComponent.h"
#include "ComponentLibrary.h"
#include <algorithm>

InputComponent::InputComponent(Object& parent, int inputSource)
    : Component(parent)
    , inputManager(InputManager::getInstance())
    , configName("") // Default: use default config for input source
    , networkInputActive(false)
{
    inputSources.push_back(inputSource);
    // Initialize network input values to 0
    for (int i = 0; i < 7; ++i) {
        networkInputValues[i] = 0.0f;
    }
}

InputComponent::InputComponent(Object& parent, const std::vector<int>& inputSources)
    : Component(parent)
    , inputSources(inputSources)
    , inputManager(InputManager::getInstance())
    , configName("") // Default: use default config for input source
    , networkInputActive(false)
{
    // Ensure we have at least one source
    if (this->inputSources.empty()) {
        this->inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
    // Initialize network input values to 0
    for (int i = 0; i < 7; ++i) {
        networkInputValues[i] = 0.0f;
    }
}

InputComponent::InputComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , inputManager(InputManager::getInstance())
    , configName("") // Default: use default config for input source
    , networkInputActive(false)
{
    if (data.contains("inputSources")) {
        inputSources = data["inputSources"].get<std::vector<int>>();
    }
    
    if (data.contains("configName")) {
        configName = data["configName"].get<std::string>();
    }
    
    // Ensure we have at least one source
    if (inputSources.empty()) {
        inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
    // Initialize network input values to 0
    for (int i = 0; i < 7; ++i) {
        networkInputValues[i] = 0.0f;
    }
}

nlohmann::json InputComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["inputSources"] = inputSources;
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
    // If network input is active, use that instead of local input
    if (networkInputActive) {
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
        if (index >= 0 && index < 7) {
            return networkInputValues[index];
        }
        return 0.0f;
    }
    
    // Return the highest value from all local sources
    float maxValue = 0.0f;
    for (int source : inputSources) {
        float value = inputManager.getInputValue(source, action, configName);
        if (value > maxValue) {
            maxValue = value;
        }
    }
    return maxValue;
}

float InputComponent::getInputFromSource(int source, GameAction action) const {
    return inputManager.getInputValue(source, action, configName);
}

bool InputComponent::isPressed(GameAction action) const {
    return getInput(action) > 0.5f;
}

void InputComponent::setInputSource(int source) {
    inputSources.clear();
    inputSources.push_back(source);
}

void InputComponent::setInputSources(const std::vector<int>& sources) {
    inputSources = sources;
    if (inputSources.empty()) {
        inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
}

void InputComponent::addInputSource(int source) {
    // Don't add duplicates
    if (std::find(inputSources.begin(), inputSources.end(), source) == inputSources.end()) {
        inputSources.push_back(source);
    }
}

void InputComponent::removeInputSource(int source) {
    auto it = std::find(inputSources.begin(), inputSources.end(), source);
    if (it != inputSources.end()) {
        inputSources.erase(it);
    }
    
    // Ensure we always have at least one source
    if (inputSources.empty()) {
        inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
}

bool InputComponent::isActive() const {
    // If network input is active, check if any network input value is non-zero
    if (networkInputActive) {
        for (int i = 0; i < 7; ++i) {
            if (networkInputValues[i] > 0.0f) {
                return true;
            }
        }
        return false;
    }
    
    // Return true if any local source is active
    for (int source : inputSources) {
        if (inputManager.isInputSourceActive(source)) {
            return true;
        }
    }
    return false;
}

bool InputComponent::isSourceActive(int source) const {
    return inputManager.isInputSourceActive(source);
}

int InputComponent::getActiveSource() const {
    // Find which source is currently providing the most input
    // Check a common action (move_up) to determine active source
    float maxValue = 0.0f;
    int activeSource = inputSources.empty() ? INPUT_SOURCE_KEYBOARD : inputSources[0];
    
    for (int source : inputSources) {
        // Check multiple actions to find the most active source
        float totalValue = 0.0f;
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_UP, configName);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_DOWN, configName);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_LEFT, configName);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_RIGHT, configName);
        totalValue += inputManager.getInputValue(source, GameAction::ACTION_INTERACT, configName);
        totalValue += inputManager.getInputValue(source, GameAction::ACTION_THROW, configName);
        
        if (totalValue > maxValue) {
            maxValue = totalValue;
            activeSource = source;
        }
    }
    
    return activeSource;
}

void InputComponent::setNetworkInput(float moveUp, float moveDown, float moveLeft, float moveRight,
                                    float actionWalk, float actionInteract, float actionThrow) {
    networkInputValues[0] = moveUp;
    networkInputValues[1] = moveDown;
    networkInputValues[2] = moveLeft;
    networkInputValues[3] = moveRight;
    networkInputValues[4] = actionWalk;
    networkInputValues[5] = actionInteract;
    networkInputValues[6] = actionThrow;
    networkInputActive = true;
}

void InputComponent::clearNetworkInput() {
    networkInputActive = false;
    for (int i = 0; i < 7; ++i) {
        networkInputValues[i] = 0.0f;
    }
}

