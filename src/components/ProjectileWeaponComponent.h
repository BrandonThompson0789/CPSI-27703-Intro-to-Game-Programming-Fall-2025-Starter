#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <random>
#include <box2d/box2d.h>

class BodyComponent;

/**
 * Component that fires a bullet using Box2D raycast when the object is used.
 * Supports range, accuracy (spread), piercing, damage, and visual rendering.
 */
class ProjectileWeaponComponent : public Component {
public:
    ProjectileWeaponComponent(Object& parent);
    ProjectileWeaponComponent(Object& parent, const nlohmann::json& data);
    ~ProjectileWeaponComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "ProjectileWeaponComponent"; }

    void use(Object& instigator) override;

private:
    struct BulletTrail {
        float startX;
        float startY;
        float endX;
        float endY;
        float duration;
        float elapsed;
        bool active;
    };

    struct HitInfo {
        b2RayResult hit;
        float hitX;
        float hitY;
    };
    
    void fireBullet(Object& instigator);
    std::vector<HitInfo> performRaycast(float startX, float startY, float dirX, float dirY, float range);
    void processHit(const b2RayResult& hit, float hitX, float hitY, Object& instigator, int pierceIndex);
    void createHitSprite(float x, float y);

    // Configuration
    float range;
    float accuracy; // 0.0 = perfect, 1.0 = maximum spread (in degrees)
    int piercingCount;
    bool canPierceStatic;
    float damage;
    std::string hitSpriteName;
    float hitSpriteWidth;
    float hitSpriteHeight;
    float hitSpriteDuration; // How long hit sprite stays visible
    float offsetX;
    float offsetY;
    float offsetAngle; // Additional angle offset from object's angle (degrees)

    // Rendering
    std::vector<BulletTrail> activeTrails;
    float trailDuration; // How long trails are visible
    uint8_t trailColorR;
    uint8_t trailColorG;
    uint8_t trailColorB;
    uint8_t trailColorA;

    // Accuracy randomness
    mutable std::mt19937 rng;
};

