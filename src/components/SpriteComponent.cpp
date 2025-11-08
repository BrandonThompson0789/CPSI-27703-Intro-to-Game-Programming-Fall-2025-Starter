#include "SpriteComponent.h"
#include "ComponentLibrary.h"
#include "../SpriteManager.h"
#include "../Engine.h"
#include "../Object.h"
#include "../components/BodyComponent.h"
#include <algorithm>
#include <random>

namespace {
float randomAngleDegrees() {
    static thread_local std::mt19937 rng(std::random_device{}());
    static std::uniform_real_distribution<float> distribution(0.0f, 360.0f);
    return distribution(rng);
}
}

SpriteComponent::SpriteComponent(Object& parent, const std::string& spriteNameParam, bool animate, bool loop, int killAfterLoopsParam)
    : Component(parent), 
      spriteName(spriteNameParam),
      currentFrame(0),
      animating(animate),
      looping(loop),
      animationSpeed(10.0f),
      animationTimer(0.0f),
      flipFlags(SDL_FLIP_NONE),
      alpha(255),
      killAfterLoops(killAfterLoopsParam),
      completedLoops(0),
      randomizeAnglePerFrame(false) {
}

SpriteComponent::SpriteComponent(Object& parent, const nlohmann::json& data)
    : Component(parent),
      spriteName(""),
      currentFrame(0),
      animating(false),
      looping(false),
      animationSpeed(10.0f),
      animationTimer(0.0f),
      flipFlags(SDL_FLIP_NONE),
      alpha(255),
      killAfterLoops(-1),
      completedLoops(0),
      randomizeAnglePerFrame(false) {
    
    if (data.contains("spriteName")) spriteName = data["spriteName"].get<std::string>();
    if (data.contains("currentFrame")) currentFrame = data["currentFrame"].get<int>();
    if (data.contains("animating")) animating = data["animating"].get<bool>();
    if (data.contains("looping")) looping = data["looping"].get<bool>();
    if (data.contains("animationSpeed")) animationSpeed = data["animationSpeed"].get<float>();
    if (data.contains("animationTimer")) animationTimer = data["animationTimer"].get<float>();
    if (data.contains("flipFlags")) flipFlags = static_cast<SDL_RendererFlip>(data["flipFlags"].get<int>());
    if (data.contains("alpha")) alpha = data["alpha"].get<uint8_t>();
    if (data.contains("killAfterLoops")) killAfterLoops = data["killAfterLoops"].get<int>();
    if (data.contains("completedLoops")) completedLoops = data["completedLoops"].get<int>();
    if (data.contains("randomizeAnglePerFrame")) randomizeAnglePerFrame = data["randomizeAnglePerFrame"].get<bool>();
    
    // Load local position (for objects without BodyComponent)
    if (data.contains("posX")) localX = data["posX"].get<float>();
    if (data.contains("posY")) localY = data["posY"].get<float>();
    if (data.contains("angle")) localAngle = data["angle"].get<float>();
    
    // Load tiling options
    if (data.contains("tiled")) tiled = data["tiled"].get<bool>();
    if (data.contains("tileWidth")) tileWidth = data["tileWidth"].get<float>();
    if (data.contains("tileHeight")) tileHeight = data["tileHeight"].get<float>();
    if (data.contains("renderWidth")) renderWidth = data["renderWidth"].get<float>();
    if (data.contains("renderHeight")) renderHeight = data["renderHeight"].get<float>();
}

nlohmann::json SpriteComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["spriteName"] = spriteName;
    j["currentFrame"] = currentFrame;
    j["animating"] = animating;
    j["looping"] = looping;
    j["animationSpeed"] = animationSpeed;
    j["animationTimer"] = animationTimer;
    j["flipFlags"] = static_cast<int>(flipFlags);
    j["alpha"] = alpha;
    if (killAfterLoops >= 0) {
        j["killAfterLoops"] = killAfterLoops;
    }
    if (completedLoops > 0) {
        j["completedLoops"] = completedLoops;
    }
    
    // Save local position (for objects without BodyComponent)
    j["posX"] = localX;
    j["posY"] = localY;
    j["angle"] = localAngle;
    
    // Save tiling options
    if (tiled) {
        j["tiled"] = tiled;
        if (tileWidth > 0) j["tileWidth"] = tileWidth;
        if (tileHeight > 0) j["tileHeight"] = tileHeight;
    }
    if (renderWidth > 0) j["renderWidth"] = renderWidth;
    if (renderHeight > 0) j["renderHeight"] = renderHeight;
    if (randomizeAnglePerFrame) {
        j["randomizeAnglePerFrame"] = randomizeAnglePerFrame;
    }
    
    return j;
}

// Register this component type with the library
static ComponentRegistrar<SpriteComponent> registrar("SpriteComponent");

void SpriteComponent::update(float deltaTime) {
    if (!animating) {
        return;
    }

    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (!spriteData || spriteData->getFrameCount() <= 1) {
        return;
    }

    // Update animation timer
    animationTimer += animationSpeed * deltaTime;

    // Advance to next frame
    if (animationTimer >= 1.0f) {
        animationTimer -= 1.0f;
        int previousFrame = currentFrame;
        currentFrame++;

        if (currentFrame >= spriteData->getFrameCount()) {
            completedLoops++;
            bool shouldMarkForDeath = (killAfterLoops >= 0 && completedLoops >= killAfterLoops);

            if (looping) {
                currentFrame = 0;
            } else {
                currentFrame = spriteData->getFrameCount() - 1;
                animating = false;
            }

            if (shouldMarkForDeath) {
                parent().markForDeath();
            }
        }

        if (randomizeAnglePerFrame && currentFrame != previousFrame) {
            applyRandomAngle();
        }
    }
}

void SpriteComponent::draw() {
    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (!spriteData) {
        return;
    }
    SpriteFrame frameData = spriteData->getFrame(currentFrame);
    Engine* engine = Object::getEngine();
    if (!engine) {
        return;
    }

    // Get position and determine render size
    float x = 0.0f, y = 0.0f, angle = 0.0f;
    float actualRenderWidth = renderWidth;
    float actualRenderHeight = renderHeight;
    
    if (auto* body = parent().getComponent<BodyComponent>()) {
        // Use physics body position
        std::tie(x, y, angle) = body->getPosition();
        
        // If renderWidth/Height not specified, use BodyComponent's fixture size
        if (actualRenderWidth == 0.0f || actualRenderHeight == 0.0f) {
            auto [fixWidth, fixHeight] = body->getFixtureSize();
            if (actualRenderWidth == 0.0f) actualRenderWidth = fixWidth;
            if (actualRenderHeight == 0.0f) actualRenderHeight = fixHeight;
        }
    } else {
        // Use local position (for objects without physics)
        x = localX;
        y = localY;
        angle = localAngle;
    }

    if (actualRenderWidth <= 0.0f) {
        actualRenderWidth = frameData.w > 0 ? static_cast<float>(frameData.w) : 1.0f;
    }
    if (actualRenderHeight <= 0.0f) {
        actualRenderHeight = frameData.h > 0 ? static_cast<float>(frameData.h) : 1.0f;
    }

    float scale = engine->getCameraScale();
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    SDL_FPoint screenPos = engine->worldToScreen(x, y);
    float screenWidth = std::max(actualRenderWidth * scale, 1.0f);
    float screenHeight = std::max(actualRenderHeight * scale, 1.0f);

    // Render the sprite - tiled or normal
    if (tiled && actualRenderWidth > 0 && actualRenderHeight > 0) {
        // Tiled rendering - repeat texture instead of stretching
        float baseTileWidth = tileWidth > 0.0f ? tileWidth : static_cast<float>(frameData.w);
        float baseTileHeight = tileHeight > 0.0f ? tileHeight : static_cast<float>(frameData.h);
        float screenTileWidth = std::max(baseTileWidth * scale, 1.0f);
        float screenTileHeight = std::max(baseTileHeight * scale, 1.0f);
        SpriteManager::getInstance().renderSpriteTiled(
            spriteName,
            currentFrame,
            screenPos.x,
            screenPos.y,
            screenWidth,
            screenHeight,
            screenTileWidth,
            screenTileHeight,
            angle,
            flipFlags,
            alpha
        );
    } else if (actualRenderWidth > 0 && actualRenderHeight > 0) {
        SpriteManager::getInstance().renderSprite(
            spriteName,
            currentFrame,
            screenPos.x,
            screenPos.y,
            screenWidth,
            screenHeight,
            angle,
            flipFlags,
            alpha
        );
    } else {
        // Normal rendering (use sprite's natural size)
        SpriteManager::getInstance().renderSprite(
            spriteName, 
            currentFrame, 
            screenPos.x,
            screenPos.y,
            screenWidth,
            screenHeight,
            angle, 
            flipFlags, 
            alpha
        );
    }
}

void SpriteComponent::setCurrentSprite(const std::string& spriteName) {
    this->spriteName = spriteName;
    completedLoops = 0;
    if (randomizeAnglePerFrame) {
        applyRandomAngle();
    }
}

void SpriteComponent::setFrame(int frameIndex) {
    int previousFrame = currentFrame;
    currentFrame = frameIndex;
    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (spriteData && currentFrame >= spriteData->getFrameCount()) {
        currentFrame = 0;
    }
    if (randomizeAnglePerFrame && currentFrame != previousFrame) {
        applyRandomAngle();
    }
}

void SpriteComponent::setAnimationSpeed(float framesPerSecond) {
    animationSpeed = framesPerSecond;
}

void SpriteComponent::playAnimation(bool loop) {
    animating = true;
    looping = loop;
    animationTimer = 0.0f;
    completedLoops = 0;
    if (randomizeAnglePerFrame) {
        applyRandomAngle();
    }
}

void SpriteComponent::stopAnimation() {
    animating = false;
    animationTimer = 0.0f;
}

void SpriteComponent::setKillAfterLoops(int loops) {
    killAfterLoops = (loops < 0) ? -1 : loops;
    if (killAfterLoops >= 0 && completedLoops > killAfterLoops) {
        completedLoops = killAfterLoops;
    }
}

void SpriteComponent::clearKillAfterLoops() {
    killAfterLoops = -1;
    completedLoops = 0;
}

void SpriteComponent::setRandomizeAnglePerFrame(bool enable) {
    randomizeAnglePerFrame = enable;
    if (randomizeAnglePerFrame) {
        applyRandomAngle();
    }
}

void SpriteComponent::setFlipHorizontal(bool flip) {
    if (flip) {
        flipFlags = static_cast<SDL_RendererFlip>(flipFlags | SDL_FLIP_HORIZONTAL);
    } else {
        flipFlags = static_cast<SDL_RendererFlip>(flipFlags & ~SDL_FLIP_HORIZONTAL);
    }
}

void SpriteComponent::setFlipVertical(bool flip) {
    if (flip) {
        flipFlags = static_cast<SDL_RendererFlip>(flipFlags | SDL_FLIP_VERTICAL);
    } else {
        flipFlags = static_cast<SDL_RendererFlip>(flipFlags & ~SDL_FLIP_VERTICAL);
    }
}

void SpriteComponent::setAlpha(uint8_t alphaValue) {
    alpha = alphaValue;
}

void SpriteComponent::setPosition(float x, float y, float angle) {
    localX = x;
    localY = y;
    localAngle = angle;
}

std::tuple<float, float, float> SpriteComponent::getPosition() const {
    // Prefer BodyComponent position if available
    if (auto* body = parent().getComponent<BodyComponent>()) {
        return body->getPosition();
    }
    return std::make_tuple(localX, localY, localAngle);
}

void SpriteComponent::setAngle(float angle) {
    if (auto* body = parent().getComponent<BodyComponent>()) {
        auto [x, y, _] = body->getPosition();
        body->setPosition(x, y, angle);
    } else {
        localAngle = angle;
    }
}

float SpriteComponent::getAngle() const {
    auto [posX, posY, currentAngle] = getPosition();
    (void)posX;
    (void)posY;
    return currentAngle;
}

void SpriteComponent::setTiled(bool isTiled, float tWidth, float tHeight) {
    tiled = isTiled;
    tileWidth = tWidth;
    tileHeight = tHeight;
}

void SpriteComponent::setRenderSize(float width, float height) {
    renderWidth = width;
    renderHeight = height;
}

void SpriteComponent::applyRandomAngle() {
    setAngle(randomAngleDegrees());
}

