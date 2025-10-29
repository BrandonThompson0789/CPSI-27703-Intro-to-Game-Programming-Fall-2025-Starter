#include "InputComponent.h"
#include "ComponentLibrary.h"
#include <algorithm>

InputComponent::InputComponent(Object& parent, int inputSource)
    : Component(parent)
    , inputManager(InputManager::getInstance())
{
    inputSources.push_back(inputSource);
}

InputComponent::InputComponent(Object& parent, const std::vector<int>& inputSources)
    : Component(parent)
    , inputSources(inputSources)
    , inputManager(InputManager::getInstance())
{
    // Ensure we have at least one source
    if (this->inputSources.empty()) {
        this->inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
}

InputComponent::InputComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , inputManager(InputManager::getInstance())
{
    if (data.contains("inputSources")) {
        inputSources = data["inputSources"].get<std::vector<int>>();
    }
    
    // Ensure we have at least one source
    if (inputSources.empty()) {
        inputSources.push_back(INPUT_SOURCE_KEYBOARD);
    }
}

nlohmann::json InputComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["inputSources"] = inputSources;
    return j;
}

// Register this component type with the library
static ComponentRegistrar<InputComponent> registrar("InputComponent");

void InputComponent::update() {
    // Input is updated by InputManager in Engine
    // This component just provides easy access to the data
}

float InputComponent::getInput(GameAction action) const {
    // Return the highest value from all sources
    float maxValue = 0.0f;
    for (int source : inputSources) {
        float value = inputManager.getInputValue(source, action);
        if (value > maxValue) {
            maxValue = value;
        }
    }
    return maxValue;
}

float InputComponent::getInputFromSource(int source, GameAction action) const {
    return inputManager.getInputValue(source, action);
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
    // Return true if any source is active
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
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_UP);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_DOWN);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_LEFT);
        totalValue += inputManager.getInputValue(source, GameAction::MOVE_RIGHT);
        totalValue += inputManager.getInputValue(source, GameAction::ACTION_INTERACT);
        totalValue += inputManager.getInputValue(source, GameAction::ACTION_THROW);
        
        if (totalValue > maxValue) {
            maxValue = totalValue;
            activeSource = source;
        }
    }
    
    return activeSource;
}

