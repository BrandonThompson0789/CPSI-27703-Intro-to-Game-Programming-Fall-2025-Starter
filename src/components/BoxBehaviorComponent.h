#pragma once
#include "Component.h"
#include <nlohmann/json.hpp>

// Forward declarations
class BodyComponent;

class BoxBehaviorComponent : public Component {
public:
    BoxBehaviorComponent(Object& parent);
    BoxBehaviorComponent(Object& parent, const nlohmann::json& data);
    virtual ~BoxBehaviorComponent() = default;
    
    void update(float deltaTime) override;
    void draw() override {}
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "BoxBehaviorComponent"; }
    
private:
    BodyComponent* body;
    float pushForce{500.0f}; // Force applied when pushed (in pixels per second)
};
