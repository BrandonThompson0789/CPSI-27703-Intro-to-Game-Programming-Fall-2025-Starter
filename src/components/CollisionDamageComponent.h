#pragma once

#include "Component.h"

#include <nlohmann/json.hpp>
#include <box2d/box2d.h>

class Object;

class CollisionDamageComponent : public Component {
public:
    explicit CollisionDamageComponent(Object& parent);
    CollisionDamageComponent(Object& parent, const nlohmann::json& data);

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "CollisionDamageComponent"; }

    float calculateDamage(const Object& target, float selfMass, float approachSpeed) const;
    bool canAffectBodyType(b2BodyType type) const;

private:
    void initializeFromJson(const nlohmann::json& data);

    bool enabled = true;
    float baseDamage = 0.0f;
    float kineticScale = 1.0f;
    float minApproachSpeed = 0.0f;
    float maxDamage = -1.0f; // negative => no cap
    bool affectDynamic = true;
    bool affectKinematic = true;
    bool affectStatic = true;
};

