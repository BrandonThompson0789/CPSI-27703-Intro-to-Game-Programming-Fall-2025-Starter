#pragma once
#include "Component.h"
#include <box2d/box2d.h>
#include <tuple>
#include <cmath>
#include <nlohmann/json.hpp>

class Object;
class BodyComponent : public Component {
public:
    BodyComponent(Object& parent);
    BodyComponent(Object& parent, float drag);
    BodyComponent(Object& parent, const nlohmann::json& data);
    ~BodyComponent() override;
    
    void update(float deltaTime) override;
    void draw() override;
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "BodyComponent"; }
    
    // Interface methods (maintain compatibility)
    void setPosition(float x, float y, float angle);
    void setVelocity(float x, float y, float angle);
    void modVelocity(float x, float y, float angle);
    std::tuple<float,float,float> getPosition();
    std::tuple<float,float,float> getVelocity();
    
    // Get fixture dimensions in pixels (width, height)
    std::tuple<float, float> getFixtureSize() const;
    
    // Box2D access (v3.x uses handles/IDs)
    b2BodyId getBodyId() { return bodyId; }

private:
    b2BodyId bodyId;
    
    // Helper methods
    void createBodyFromJson(const nlohmann::json& data);
    void createDefaultBody(float posX = 0.0f, float posY = 0.0f, float angle = 0.0f);
    void createFixtureFromJson(const nlohmann::json& fixtureData);
    b2BodyType parseBodyType(const std::string& typeStr);
    std::string bodyTypeToString(b2BodyType type) const;
};
