#pragma once

#include "Component.h"
#include <string>

class ExplodeOnDeathComponent : public Component {
public:
    ExplodeOnDeathComponent(Object& parent);
    ExplodeOnDeathComponent(Object& parent, const nlohmann::json& data);
    ~ExplodeOnDeathComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "ExplodeOnDeathComponent"; }

    void setExplosionSprite(const std::string& sprite);
    void setExplosionSize(float width, float height);
    void setExplosionLoops(int loops);
    void setRandomizeAngle(bool randomize);

protected:
    void onParentDeath() override;

private:
    std::string explosionSprite;
    float explosionWidth;
    float explosionHeight;
    int loopsBeforeDeath;
    bool randomizeAngleEachFrame;
    bool triggered;

    void spawnExplosion();
};


