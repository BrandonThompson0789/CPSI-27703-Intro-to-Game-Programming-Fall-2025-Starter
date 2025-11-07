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
    void setPosition(float x, float y, float angleDegrees);
    void setVelocity(float x, float y, float angularVelocityDegreesPerSecond);
    void modVelocity(float x, float y, float angularVelocityDegreesPerSecond);
    // Returns (x, y, angleDegrees)
    std::tuple<float,float,float> getPosition();
    // Returns (velX, velY, angularVelocityDegreesPerSecond)
    std::tuple<float,float,float> getVelocity();
    
    // Get fixture dimensions in pixels (width, height)
    std::tuple<float, float> getFixtureSize() const;

    int getMaterialId() const { return dominantMaterialId; }
    
    // Box2D access (v3.x uses handles/IDs)
    b2BodyId getBodyId() const { return bodyId; }

private:
    b2BodyId bodyId;
    int dominantMaterialId = 0;
    bool hasExplicitMaterial = false;
    
    // Helper methods
    void createBodyFromJson(const nlohmann::json& data);
    void createDefaultBody(float posX = 0.0f, float posY = 0.0f, float angle = 0.0f);
    void createFixtureFromJson(const nlohmann::json& fixtureData);
    void configureShapeDef(b2ShapeDef& shapeDef, int materialId) const;
    void applyShapeConfiguration(b2ShapeId shapeId, int materialId);
    b2BodyType parseBodyType(const std::string& typeStr);
    std::string bodyTypeToString(b2BodyType type) const;
};
