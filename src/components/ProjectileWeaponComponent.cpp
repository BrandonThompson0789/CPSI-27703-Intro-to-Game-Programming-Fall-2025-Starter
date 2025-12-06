#include "ProjectileWeaponComponent.h"
#include "ComponentLibrary.h"
#include "../Engine.h"
#include "../Object.h"
#include "BodyComponent.h"
#include "HealthComponent.h"
#include "SpriteComponent.h"
#include <box2d/box2d.h>
#include <random>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstring>

ProjectileWeaponComponent::ProjectileWeaponComponent(Object& parent)
    : Component(parent)
    , range(500.0f)
    , accuracy(0.0f)
    , piercingCount(0)
    , canPierceStatic(false)
    , damage(10.0f)
    , hitSpriteName("explosion")
    , hitSpriteWidth(32.0f)
    , hitSpriteHeight(32.0f)
    , hitSpriteDuration(0.1f)
    , offsetX(0.0f)
    , offsetY(0.0f)
    , offsetAngle(0.0f)
    , trailDuration(0.1f)
    , trailColorR(255)
    , trailColorG(255)
    , trailColorB(0)
    , trailColorA(255)
    , rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())) {
}

ProjectileWeaponComponent::ProjectileWeaponComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , range(data.value("range", 500.0f))
    , accuracy(data.value("accuracy", 0.0f))
    , piercingCount(data.value("piercingCount", 0))
    , canPierceStatic(data.value("canPierceStatic", false))
    , damage(data.value("damage", 10.0f))
    , hitSpriteName(data.value("hitSprite", "explosion"))
    , hitSpriteWidth(data.value("hitSpriteWidth", 32.0f))
    , hitSpriteHeight(data.value("hitSpriteHeight", 32.0f))
    , hitSpriteDuration(data.value("hitSpriteDuration", 0.1f))
    , offsetX(data.value("offsetX", 0.0f))
    , offsetY(data.value("offsetY", 0.0f))
    , offsetAngle(data.value("offsetAngle", 0.0f))
    , trailDuration(data.value("trailDuration", 0.1f))
    , trailColorR(255)
    , trailColorG(255)
    , trailColorB(0)
    , trailColorA(255)
    , rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())) {
    
    // Parse trail color if provided
    if (data.contains("trailColor") && data["trailColor"].is_object()) {
        trailColorR = data["trailColor"].value("r", 255);
        trailColorG = data["trailColor"].value("g", 255);
        trailColorB = data["trailColor"].value("b", 0);
        trailColorA = data["trailColor"].value("a", 255);
    }
}

void ProjectileWeaponComponent::update(float deltaTime) {
    // Update active bullet trails
    for (auto& trail : activeTrails) {
        if (trail.active) {
            trail.elapsed += deltaTime;
            if (trail.elapsed >= trail.duration) {
                trail.active = false;
            }
        }
    }
    
    // Remove inactive trails
    activeTrails.erase(
        std::remove_if(activeTrails.begin(), activeTrails.end(),
            [](const BulletTrail& t) { return !t.active; }),
        activeTrails.end());
}

void ProjectileWeaponComponent::draw() {
    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    SDL_Renderer* renderer = engine->getRenderer();
    if (!renderer) {
        return;
    }

    // Save current draw color
    Uint8 oldR, oldG, oldB, oldA;
    SDL_GetRenderDrawColor(renderer, &oldR, &oldG, &oldB, &oldA);

    // Draw all active bullet trails
    SDL_SetRenderDrawColor(renderer, trailColorR, trailColorG, trailColorB, trailColorA);
    for (const auto& trail : activeTrails) {
        if (trail.active) {
            // Convert world coordinates to screen coordinates
            float alpha = trail.elapsed / trail.duration;
            uint8_t currentAlpha = static_cast<uint8_t>(trailColorA * (1.0f - alpha));
            SDL_SetRenderDrawColor(renderer, trailColorR, trailColorG, trailColorB, currentAlpha);
            
            // Get camera state for coordinate conversion
            auto cameraState = engine->getCameraState();
            float scale = cameraState.scale;
            
            // Convert to screen coordinates
            float screenStartX = (trail.startX - cameraState.viewMinX) * scale;
            float screenStartY = (trail.startY - cameraState.viewMinY) * scale;
            float screenEndX = (trail.endX - cameraState.viewMinX) * scale;
            float screenEndY = (trail.endY - cameraState.viewMinY) * scale;
            
            SDL_RenderDrawLineF(renderer, screenStartX, screenStartY, screenEndX, screenEndY);
        }
    }

    // Restore original draw color
    SDL_SetRenderDrawColor(renderer, oldR, oldG, oldB, oldA);
}

nlohmann::json ProjectileWeaponComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["range"] = range;
    j["accuracy"] = accuracy;
    j["piercingCount"] = piercingCount;
    j["canPierceStatic"] = canPierceStatic;
    j["damage"] = damage;
    j["hitSprite"] = hitSpriteName;
    j["hitSpriteWidth"] = hitSpriteWidth;
    j["hitSpriteHeight"] = hitSpriteHeight;
    j["hitSpriteDuration"] = hitSpriteDuration;
    j["offsetX"] = offsetX;
    j["offsetY"] = offsetY;
    j["offsetAngle"] = offsetAngle;
    j["trailDuration"] = trailDuration;
    
    nlohmann::json color;
    color["r"] = trailColorR;
    color["g"] = trailColorG;
    color["b"] = trailColorB;
    color["a"] = trailColorA;
    j["trailColor"] = color;
    
    return j;
}

void ProjectileWeaponComponent::use(Object& instigator) {
    fireBullet(instigator);
}

void ProjectileWeaponComponent::fireBullet(Object& instigator) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    // Get position and angle from parent object
    float posX = 0.0f;
    float posY = 0.0f;
    float angleDeg = 0.0f;

    BodyComponent* body = parent().getComponent<BodyComponent>();
    if (body) {
        std::tie(posX, posY, angleDeg) = body->getPosition();
    } else {
        SpriteComponent* sprite = parent().getComponent<SpriteComponent>();
        if (sprite) {
            std::tie(posX, posY, angleDeg) = sprite->getPosition();
        }
    }

    // Apply offset (relative to object's angle)
    float angleRad = Engine::degreesToRadians(angleDeg + offsetAngle);
    float offsetDirX = std::sin(angleRad);
    float offsetDirY = -std::cos(angleRad);
    
    // Perpendicular for offset Y
    float perpX = -offsetDirY;
    float perpY = offsetDirX;
    
    float startX = posX + offsetDirX * offsetX + perpX * offsetY;
    float startY = posY + offsetDirY * offsetX + perpY * offsetY;

    // Calculate direction with accuracy spread
    float spreadAngleDeg = 0.0f;
    if (accuracy > 0.0f) {
        std::uniform_real_distribution<float> spreadDist(-accuracy, accuracy);
        spreadAngleDeg = spreadDist(rng);
    }
    
    float finalAngleDeg = angleDeg + offsetAngle + spreadAngleDeg;
    float finalAngleRad = Engine::degreesToRadians(finalAngleDeg);
    
    float dirX = std::sin(finalAngleRad);
    float dirY = -std::cos(finalAngleRad);

    // Perform raycast(s) for piercing
    std::vector<HitInfo> hits = performRaycast(startX, startY, dirX, dirY, range);
    
    if (hits.empty()) {
        // No hits, draw trail to max range
        BulletTrail trail;
        trail.startX = startX;
        trail.startY = startY;
        trail.endX = startX + dirX * range;
        trail.endY = startY + dirY * range;
        trail.duration = trailDuration;
        trail.elapsed = 0.0f;
        trail.active = true;
        activeTrails.push_back(trail);
        return;
    }

    // Process hits sequentially, drawing trails between them
    float currentX = startX;
    float currentY = startY;
    
    for (size_t i = 0; i < hits.size(); ++i) {
        const auto& hitInfo = hits[i];
        float hitX = hitInfo.hitX;
        float hitY = hitInfo.hitY;
        
        // Draw trail from current position to hit point
        BulletTrail trail;
        trail.startX = currentX;
        trail.startY = currentY;
        trail.endX = hitX;
        trail.endY = hitY;
        trail.duration = trailDuration;
        trail.elapsed = 0.0f;
        trail.active = true;
        activeTrails.push_back(trail);
        
        // Process the hit (damage, sprite)
        processHit(hitInfo.hit, hitX, hitY, instigator, static_cast<int>(i));
        
        // Move to hit point for next trail segment
        currentX = hitX;
        currentY = hitY;
    }
    
    // Trail rendering stops at the last hit - no need to draw beyond that
}

std::vector<ProjectileWeaponComponent::HitInfo> ProjectileWeaponComponent::performRaycast(float startX, float startY, float dirX, float dirY, float maxRange) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        return {};
    }

    b2WorldId worldId = engine->getPhysicsWorld();
    if (B2_IS_NULL(worldId)) {
        return {};
    }

    std::vector<ProjectileWeaponComponent::HitInfo> allHits;
    float currentX = startX;
    float currentY = startY;
    float remainingRange = maxRange;
    int maxIterations = piercingCount + 1; // Prevent infinite loops

    // Use a vector to track already-hit shapes to avoid hitting the same shape twice
    // Note: b2ShapeId cannot be used in std::unordered_set, so we use a vector and check manually
    std::vector<b2ShapeId> hitShapes;

    // Keep casting rays until we hit a non-sensor or run out of range/iterations
    for (int iteration = 0; iteration < maxIterations && remainingRange > 0.0f; ++iteration) {
        b2Vec2 rayOrigin = {
            currentX * Engine::PIXELS_TO_METERS,
            currentY * Engine::PIXELS_TO_METERS
        };

        b2Vec2 rayTranslation = {
            dirX * remainingRange * Engine::PIXELS_TO_METERS,
            dirY * remainingRange * Engine::PIXELS_TO_METERS
        };

        b2QueryFilter filter = b2DefaultQueryFilter();
        b2RayResult result = b2World_CastRayClosest(worldId, rayOrigin, rayTranslation, filter);

        if (!result.hit || B2_IS_NULL(result.shapeId)) {
            break; // No more hits
        }

        // Calculate hit distance
        float hitDistance = result.fraction * remainingRange;
        
        // Skip sensor fixtures - move past them and continue raycast
        if (b2Shape_IsSensor(result.shapeId)) {
            // Move past the sensor and continue the raycast
            currentX += dirX * hitDistance;
            currentY += dirY * hitDistance;
            remainingRange -= hitDistance;
            // Don't increment iteration count for sensors, just continue
            iteration--; // Decrement so we don't count sensor hits against iteration limit
            continue; // Continue raycast from new position
        }

        // Skip if we've already hit this shape
        // In Box2D v3, b2ShapeId is a handle, so we compare by checking if they're the same handle
        bool alreadyHit = false;
        for (const auto& hitShape : hitShapes) {
            if (!B2_IS_NULL(hitShape) && !B2_IS_NULL(result.shapeId)) {
                // Compare shape IDs - in Box2D v3, we can use memcmp or compare the handle directly
                // Since b2ShapeId is a struct with index and revision, we compare both
                if (memcmp(&hitShape, &result.shapeId, sizeof(b2ShapeId)) == 0) {
                    alreadyHit = true;
                    break;
                }
            }
        }
        if (alreadyHit) {
            break;
        }
        hitShapes.push_back(result.shapeId);

        // Calculate hit point using the fraction from raycast result
        float hitX = currentX + dirX * hitDistance;
        float hitY = currentY + dirY * hitDistance;

        // Store hit with position for processing
        ProjectileWeaponComponent::HitInfo hitInfo;
        hitInfo.hit = result;
        hitInfo.hitX = hitX;
        hitInfo.hitY = hitY;
        allHits.push_back(hitInfo);
        
        // Update for next iteration
        currentX = hitX;
        currentY = hitY;
        remainingRange -= hitDistance;
        
        // Check if we should continue (based on body type)
        b2BodyId hitBodyId = b2Shape_GetBody(result.shapeId);
        b2BodyType bodyType = b2Body_GetType(hitBodyId);
        
        if (bodyType == b2_staticBody && !canPierceStatic) {
            break; // Can't pierce static
        }
        
        if (iteration >= piercingCount) {
            break; // Used up all pierces
        }
    }

    return allHits;
}

void ProjectileWeaponComponent::processHit(const b2RayResult& hit, float hitX, float hitY, Object& instigator, int pierceIndex) {
    (void)pierceIndex; // Not used currently

    // Create hit sprite
    createHitSprite(hitX, hitY);

    // Apply damage if object has health
    b2BodyId hitBodyId = b2Shape_GetBody(hit.shapeId);
    void* userData = b2Body_GetUserData(hitBodyId);
    if (!userData) {
        return;
    }

    Object* hitObject = static_cast<Object*>(userData);
    if (!hitObject || hitObject == &parent()) {
        return; // Don't damage self
    }

    HealthComponent* health = hitObject->getComponent<HealthComponent>();
    if (health && damage > 0.0f) {
        health->applyDamage(damage, "projectile", &instigator);
    }
}

void ProjectileWeaponComponent::createHitSprite(float x, float y) {
    if (hitSpriteName.empty()) {
        return;
    }

    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    auto hitSpriteObject = std::make_unique<Object>();
    hitSpriteObject->setName(parent().getName().empty() ? "bullet_hit" : parent().getName() + "_hit");

    // Calculate approximate loops from duration (assuming ~10 fps default animation speed)
    // If duration is very short, use 1 loop; otherwise estimate based on typical animation
    int loops = 1;
    if (hitSpriteDuration > 0.1f) {
        // Rough estimate: assume ~10 fps, so each loop is ~0.1s for a typical sprite
        loops = static_cast<int>(std::ceil(hitSpriteDuration / 0.1f));
    }
    
    auto* spriteComponent = hitSpriteObject->addComponent<SpriteComponent>(
        hitSpriteName,
        true,  // animate
        false, // loop (one shot)
        loops); // kill after loops

    if (spriteComponent) {
        spriteComponent->setRenderSize(hitSpriteWidth, hitSpriteHeight);
        spriteComponent->setPosition(x, y, 0.0f);
        spriteComponent->playAnimation(false); // Don't loop
    }

    engine->queueObject(std::move(hitSpriteObject));
}

static ComponentRegistrar<ProjectileWeaponComponent> registrar("ProjectileWeaponComponent");

