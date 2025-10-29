#pragma once
#include "Component.h"
#include <tuple>
#include <cmath>
#include <nlohmann/json.hpp>

class Object;
class BodyComponent : public Component {
public:
    BodyComponent(Object& parent);
    BodyComponent(Object& parent, float drag);
    BodyComponent(Object& parent, const nlohmann::json& data);
    ~BodyComponent() = default;
    
    void update(float deltaTime) override;
    void draw() override;
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "BodyComponent"; }
    
    void setPosition(float x, float y, float angle);
    void setVelocity(float x, float y, float angle);
    void modVelocity(float x, float y, float angle);
    std::tuple<float,float,float> getPosition();
    std::tuple<float,float,float> getVelocity();

private:
    float posX{};
    float posY{};
    float angle{};
    float velX{};
    float velY{};
    float velAngle{};
    float drag{0.95f}; // Natural velocity drag per frame (0.95 = 5% reduction)
};
