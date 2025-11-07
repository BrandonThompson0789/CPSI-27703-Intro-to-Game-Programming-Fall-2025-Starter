#include "HealthComponent.h"

#include "ComponentLibrary.h"
#include "../Object.h"
#include "../PhysicsMaterial.h"

#include <algorithm>
#include <sstream>
#include <iostream>

namespace {
constexpr float EPSILON = 1e-4f;
}

HealthComponent::HealthComponent(Object& parent)
    : Component(parent) {
}

HealthComponent::HealthComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    initializeFromJson(data);
}

void HealthComponent::initializeFromJson(const nlohmann::json& data) {
    if (data.contains("maxHP")) {
        maxHP = std::max(0.0f, data["maxHP"].get<float>());
    }

    if (data.contains("currentHP")) {
        currentHP = std::clamp(data["currentHP"].get<float>(), 0.0f, maxHP > 0.0f ? maxHP : data["currentHP"].get<float>());
    } else {
        currentHP = maxHP;
    }

    impactResistance = data.value("impactResistance", impactResistance);
    damageMultiplier = data.value("damageMultiplier", damageMultiplier);
    regenPerSecond = data.value("regenPerSecond", regenPerSecond);
    regenDelay = data.value("regenDelay", regenDelay);
    receiveCollisionDamage = data.value("receiveCollisionDamage", receiveCollisionDamage);
    destroyOnDeath = data.value("destroyOnDeath", destroyOnDeath);
}

void HealthComponent::update(float deltaTime) {
    processRegen(deltaTime);
}

void HealthComponent::draw() {
    // Health has no direct rendering responsibilities
}

void HealthComponent::processRegen(float deltaTime) {
    timeSinceDamage += deltaTime;

    if (regenPerSecond <= EPSILON) {
        return;
    }

    if (currentHP <= 0.0f || currentHP >= maxHP) {
        currentHP = std::clamp(currentHP, 0.0f, maxHP);
        return;
    }

    if (timeSinceDamage < regenDelay) {
        return;
    }

    currentHP = std::min(maxHP, currentHP + regenPerSecond * deltaTime);
}

void HealthComponent::heal(float amount) {
    if (amount <= 0.0f || currentHP <= 0.0f) {
        return;
    }

    currentHP = std::min(maxHP, currentHP + amount);
}

void HealthComponent::applyDamage(float amount, const std::string& source, Object* instigator) {
    if (amount <= 0.0f) {
        return;
    }

    float adjusted = amount * damageMultiplier;
    applyDamageInternal(adjusted, source, instigator);
}

void HealthComponent::applyImpactDamage(float rawDamage, float approachSpeed, int instigatorMaterialId, Object* instigator) {
    if (!receiveCollisionDamage) {
        return;
    }

    float mitigated = rawDamage - impactResistance;
    if (mitigated <= 0.0f) {
        return;
    }

    mitigated *= damageMultiplier;
    if (mitigated <= 0.0f) {
        return;
    }

    std::ostringstream source;
    source << "impact(" << PhysicsMaterialLibrary::getMaterialName(instigatorMaterialId) << ","
           << std::max(0.0f, approachSpeed) << "mps)";

    applyDamageInternal(mitigated, source.str(), instigator);
}

void HealthComponent::applyDamageInternal(float amount, const std::string& source, Object* instigator) {
    if (amount <= 0.0f || currentHP <= 0.0f) {
        return;
    }

    currentHP = std::max(0.0f, currentHP - amount);
    timeSinceDamage = 0.0f;

    std::cout << "HealthComponent::applyDamageInternal: " << amount << " " << source << " " << instigator->getName() << std::endl;
    std::cout << "currentHP: " << currentHP << std::endl;

    if (currentHP <= 0.0f && destroyOnDeath) {
        handleDeath(instigator, source);
    }
}

void HealthComponent::handleDeath(Object* instigator, const std::string& source) {
    (void)instigator;
    (void)source;

    parent().markForDeath();
}

nlohmann::json HealthComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["maxHP"] = maxHP;
    data["currentHP"] = currentHP;
    data["impactResistance"] = impactResistance;
    data["damageMultiplier"] = damageMultiplier;
    data["regenPerSecond"] = regenPerSecond;
    data["regenDelay"] = regenDelay;
    data["receiveCollisionDamage"] = receiveCollisionDamage;
    data["destroyOnDeath"] = destroyOnDeath;
    return data;
}

// Register component
static ComponentRegistrar<HealthComponent> registrar("HealthComponent");

