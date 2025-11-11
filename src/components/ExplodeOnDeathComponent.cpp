#include "ExplodeOnDeathComponent.h"
#include "ComponentLibrary.h"
#include "SpriteComponent.h"
#include "BodyComponent.h"
#include "SoundComponent.h"
#include "../Object.h"
#include "../Engine.h"
#include <random>

namespace {
float randomAngleDegrees() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> distribution(0.0f, 360.0f);
    return distribution(rng);
}
}

ExplodeOnDeathComponent::ExplodeOnDeathComponent(Object& parent)
    : Component(parent),
      explosionSprite("explosion"),
      explosionWidth(64.0f),
      explosionHeight(64.0f),
      loopsBeforeDeath(3),
      randomizeAngleEachFrame(true),
      triggered(false) {
}

ExplodeOnDeathComponent::ExplodeOnDeathComponent(Object& parent, const nlohmann::json& data)
    : Component(parent),
      explosionSprite("explosion"),
      explosionWidth(64.0f),
      explosionHeight(64.0f),
      loopsBeforeDeath(3),
      randomizeAngleEachFrame(true),
      triggered(false) {
    if (data.contains("explosionSprite")) {
        explosionSprite = data["explosionSprite"].get<std::string>();
    }
    if (data.contains("explosionWidth")) {
        explosionWidth = data["explosionWidth"].get<float>();
    }
    if (data.contains("explosionHeight")) {
        explosionHeight = data["explosionHeight"].get<float>();
    }
    if (data.contains("loopsBeforeDeath")) {
        loopsBeforeDeath = data["loopsBeforeDeath"].get<int>();
    }
    if (data.contains("randomizeAngleEachFrame")) {
        randomizeAngleEachFrame = data["randomizeAngleEachFrame"].get<bool>();
    }
}

void ExplodeOnDeathComponent::update(float /*deltaTime*/) {
    // No continuous behavior required
}

void ExplodeOnDeathComponent::draw() {
    // Nothing to draw directly
}

nlohmann::json ExplodeOnDeathComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["explosionSprite"] = explosionSprite;
    data["explosionWidth"] = explosionWidth;
    data["explosionHeight"] = explosionHeight;
    data["loopsBeforeDeath"] = loopsBeforeDeath;
    data["randomizeAngleEachFrame"] = randomizeAngleEachFrame;
    return data;
}

void ExplodeOnDeathComponent::setExplosionSprite(const std::string& sprite) {
    explosionSprite = sprite;
}

void ExplodeOnDeathComponent::setExplosionSize(float width, float height) {
    explosionWidth = width;
    explosionHeight = height;
}

void ExplodeOnDeathComponent::setExplosionLoops(int loops) {
    loopsBeforeDeath = loops;
}

void ExplodeOnDeathComponent::setRandomizeAngle(bool randomize) {
    randomizeAngleEachFrame = randomize;
}

void ExplodeOnDeathComponent::onParentDeath() {
    if (triggered) {
        return;
    }
    triggered = true;
    
    auto* sound = parent().getComponent<SoundComponent>();
    if(sound)sound->playActionSound("explode");

    spawnExplosion();
}

void ExplodeOnDeathComponent::spawnExplosion() {
    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    float posX = 0.0f;
    float posY = 0.0f;
    float angle = 0.0f;

    if (auto* body = parent().getComponent<BodyComponent>()) {
        std::tie(posX, posY, angle) = body->getPosition();
    } else if (auto* sprite = parent().getComponent<SpriteComponent>()) {
        std::tie(posX, posY, angle) = sprite->getPosition();
    }

    auto explosionObject = std::make_unique<Object>();
    if (!parent().getName().empty()) {
        explosionObject->setName(parent().getName() + "_explosion");
    } else {
        explosionObject->setName("explosion");
    }

    auto* explosionSpriteComponent = explosionObject->addComponent<SpriteComponent>(
        explosionSprite,
        true,
        true,
        loopsBeforeDeath);

    if (explosionSpriteComponent) {
        explosionSpriteComponent->setRenderSize(explosionWidth, explosionHeight);
        float initialAngle = randomizeAngleEachFrame ? randomAngleDegrees() : angle;
        explosionSpriteComponent->setPosition(posX, posY, initialAngle);
        explosionSpriteComponent->playAnimation(true);
    }

    if (explosionSpriteComponent) {
        explosionSpriteComponent->setRandomizeAnglePerFrame(randomizeAngleEachFrame);
    }

    engine->queueObject(std::move(explosionObject));
}

static ComponentRegistrar<ExplodeOnDeathComponent> registrar("ExplodeOnDeathComponent");


