#include "CollisionDamageComponent.h"

#include "ComponentLibrary.h"
#include "../Object.h"

#include <algorithm>

namespace {
constexpr float HALF = 0.5f;
}

CollisionDamageComponent::CollisionDamageComponent(Object& parent)
    : Component(parent) {
}

CollisionDamageComponent::CollisionDamageComponent(Object& parent, const nlohmann::json& data)
    : Component(parent) {
    initializeFromJson(data);
}

void CollisionDamageComponent::initializeFromJson(const nlohmann::json& data) {
    enabled = data.value("enabled", enabled);
    baseDamage = data.value("baseDamage", baseDamage);
    kineticScale = data.value("kineticScale", kineticScale);
    minApproachSpeed = data.value("minApproachSpeed", minApproachSpeed);
    maxDamage = data.value("maxDamage", maxDamage);
    affectDynamic = data.value("affectDynamic", affectDynamic);
    affectKinematic = data.value("affectKinematic", affectKinematic);
    affectStatic = data.value("affectStatic", affectStatic);
}

void CollisionDamageComponent::update(float /*deltaTime*/) {
    // No per-frame logic required; damage is computed on impact
}

void CollisionDamageComponent::draw() {
    // This component has no rendering responsibilities
}

float CollisionDamageComponent::calculateDamage(const Object& /*target*/, float selfMass, float approachSpeed) const {
    if (!enabled) {
        return 0.0f;
    }

    if (approachSpeed < minApproachSpeed) {
        return 0.0f;
    }

    float kineticDamage = 0.0f;
    if (kineticScale > 0.0f && selfMass > 0.0f) {
        kineticDamage = HALF * selfMass * approachSpeed * approachSpeed * kineticScale;
    }

    float damage = baseDamage + kineticDamage;
    if (maxDamage >= 0.0f) {
        damage = std::min(damage, maxDamage);
    }

    return damage;
}

bool CollisionDamageComponent::canAffectBodyType(b2BodyType type) const {
    switch (type) {
        case b2_dynamicBody:
            return affectDynamic;
        case b2_kinematicBody:
            return affectKinematic;
        case b2_staticBody:
        default:
            return affectStatic;
    }
}

nlohmann::json CollisionDamageComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["enabled"] = enabled;
    data["baseDamage"] = baseDamage;
    data["kineticScale"] = kineticScale;
    data["minApproachSpeed"] = minApproachSpeed;
    data["maxDamage"] = maxDamage;
    data["affectDynamic"] = affectDynamic;
    data["affectKinematic"] = affectKinematic;
    data["affectStatic"] = affectStatic;
    return data;
}

static ComponentRegistrar<CollisionDamageComponent> registrar("CollisionDamageComponent");

