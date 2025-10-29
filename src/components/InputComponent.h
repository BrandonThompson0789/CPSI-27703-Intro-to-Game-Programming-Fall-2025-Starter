#ifndef INPUT_COMPONENT_H
#define INPUT_COMPONENT_H

#include "Component.h"
#include "../InputManager.h"
#include <vector>
#include <nlohmann/json.hpp>

class InputComponent : public Component {
public:
    // Constructor - specify which input source(s) this component listens to
    // inputSource: -1 for keyboard/mouse, 0-3 for controllers
    InputComponent(Object& parent, int inputSource = INPUT_SOURCE_KEYBOARD);
    
    // Constructor - multiple input sources (listens to all, returns highest value)
    InputComponent(Object& parent, const std::vector<int>& inputSources);
    
    // Constructor from JSON
    InputComponent(Object& parent, const nlohmann::json& data);
    
    virtual ~InputComponent() = default;
    
    void update(float deltaTime) override;
    void draw() override {} // Input component doesn't draw
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "InputComponent"; }
    
    // Get input value for a specific action (0.0 to 1.0)
    // When multiple sources are configured, returns the highest value
    float getInput(GameAction action) const;
    
    // Get input value from a specific source (useful when multiple sources are configured)
    float getInputFromSource(int source, GameAction action) const;
    
    // Convenience methods for common actions
    float getMoveUp() const { return getInput(GameAction::MOVE_UP); }
    float getMoveDown() const { return getInput(GameAction::MOVE_DOWN); }
    float getMoveLeft() const { return getInput(GameAction::MOVE_LEFT); }
    float getMoveRight() const { return getInput(GameAction::MOVE_RIGHT); }
    float getActionWalk() const { return getInput(GameAction::ACTION_WALK); }
    float getActionInteract() const { return getInput(GameAction::ACTION_INTERACT); }
    float getActionThrow() const { return getInput(GameAction::ACTION_THROW); }
    
    // Check if button is pressed (value > 0.5) from any source
    bool isPressed(GameAction action) const;
    
    // Get primary input source (first in list)
    int getInputSource() const { return inputSources.empty() ? INPUT_SOURCE_KEYBOARD : inputSources[0]; }
    
    // Get all input sources
    const std::vector<int>& getInputSources() const { return inputSources; }
    
    // Set input source (replaces all sources with a single source)
    void setInputSource(int source);
    
    // Set multiple input sources
    void setInputSources(const std::vector<int>& sources);
    
    // Add an input source to the list
    void addInputSource(int source);
    
    // Remove an input source from the list
    void removeInputSource(int source);
    
    // Check if any input source is active/connected
    bool isActive() const;
    
    // Check if a specific input source is active
    bool isSourceActive(int source) const;
    
    // Get which source is currently providing input (highest value)
    int getActiveSource() const;
    
private:
    std::vector<int> inputSources;
    InputManager& inputManager;
};

#endif // INPUT_COMPONENT_H

