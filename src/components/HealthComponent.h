#pragma once

#include "Component.h"

#include <string>

class Object;

class HealthComponent : public Component {
public:
    explicit HealthComponent(Object& parent);
    HealthComponent(Object& parent, const nlohmann::json& data);
    ~HealthComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "HealthComponent"; }

    float getCurrentHP() const { return currentHP; }
    float getMaxHP() const { return maxHP; }
    float getHealthPercent() const { return maxHP > 0.0f ? currentHP / maxHP : 0.0f; }

    float getImpactResistance() const { return impactResistance; }
    void setImpactResistance(float resistance) { impactResistance = resistance; }

    float getDamageMultiplier() const { return damageMultiplier; }
    void setDamageMultiplier(float multiplier) { damageMultiplier = multiplier; }

    bool receivesCollisionDamage() const { return receiveCollisionDamage; }
    void setReceivesCollisionDamage(bool flag) { receiveCollisionDamage = flag; }

    bool shouldDestroyOnDeath() const { return destroyOnDeath; }
    void setDestroyOnDeath(bool flag) { destroyOnDeath = flag; }

    void setRegenRate(float perSecond) { regenPerSecond = perSecond; }
    void setRegenDelay(float delaySeconds) { regenDelay = delaySeconds; }

    void heal(float amount);
    void applyDamage(float amount, const std::string& source = "generic", Object* instigator = nullptr);
    void applyImpactDamage(float rawDamage, float approachSpeed, int instigatorMaterialId, Object* instigator);

    bool isDead() const { return currentHP <= 0.0f; }

private:
    void initializeFromJson(const nlohmann::json& data);
    void processRegen(float deltaTime);
    void handleDeath(Object* instigator, const std::string& source);
    void applyDamageInternal(float amount, const std::string& source, Object* instigator);

    float maxHP = 100.0f;
    float currentHP = 100.0f;
    float impactResistance = 0.0f;
    float damageMultiplier = 1.0f;
    float regenPerSecond = 0.0f;
    float regenDelay = 0.0f;
    float timeSinceDamage = 0.0f;
    bool receiveCollisionDamage = true;
    bool destroyOnDeath = true;
};

