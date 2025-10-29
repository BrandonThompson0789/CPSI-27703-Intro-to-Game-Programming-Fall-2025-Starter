#pragma once
#include "Component.h"
#include <tuple>
#include <nlohmann/json.hpp>

class Object;
class BodyComponent : public Component {
public:
    BodyComponent(Object& parent);
    BodyComponent(Object& parent, const nlohmann::json& data);
    ~BodyComponent() = default;
    
    void update() override;
    void draw() override;
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "BodyComponent"; }
    
    void setPosition(float x, float y, float angle);
    void setVelocity(float x, float y, float angle);
    std::tuple<float,float,float> getPosition();
    std::tuple<float,float,float> getVelocity();

private:
    float posX{};
    float posY{};
    float angle{};
    float velX{};
    float velY{};
    float velAngle{};
};
