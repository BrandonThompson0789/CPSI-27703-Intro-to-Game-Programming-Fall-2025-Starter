#include "SpriteComponent.h"
#include "SpriteManager.h"
#include "../Object.h"
#include "../components/BodyComponent.h"

SpriteComponent::SpriteComponent(Object& parent, const std::string& spriteNameParam, bool animate, bool loop)
    : Component(parent), 
      spriteName(spriteNameParam),
      currentFrame(0),
      animating(animate),
      looping(loop),
      animationSpeed(10.0f),
      animationTimer(0.0f),
      flipFlags(SDL_FLIP_NONE),
      alpha(255) {
}

void SpriteComponent::update() {
    if (!animating) {
        return;
    }

    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (!spriteData || spriteData->getFrameCount() <= 1) {
        return;
    }

    // Update animation timer (assuming 60 FPS, adjust as needed)
    animationTimer += animationSpeed / 60.0f;

    // Advance to next frame
    if (animationTimer >= 1.0f) {
        animationTimer -= 1.0f;
        currentFrame++;

        if (currentFrame >= spriteData->getFrameCount()) {
            if (looping) {
                currentFrame = 0;
            } else {
                currentFrame = spriteData->getFrameCount() - 1;
                animating = false;
            }
        }
    }
}

void SpriteComponent::draw() {
    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (!spriteData) {
        return;
    }

    // Get position from parent object
    // Use "BodyComponent.h" and check if the parent has BodyComponent
    float x = 0.0f, y = 0.0f, angle = 0.0f;
    if (auto* body = parent().getComponent<BodyComponent>()) {
        std::tie(x, y, angle) = body->getPosition();
    }

    // Render the sprite at the current frame
    SpriteManager::getInstance().renderSprite(
        spriteName, 
        currentFrame, 
        static_cast<int>(x), 
        static_cast<int>(y), 
        angle, 
        flipFlags, 
        alpha
    );
}

void SpriteComponent::setFrame(int frameIndex) {
    currentFrame = frameIndex;
    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(spriteName);
    if (spriteData && currentFrame >= spriteData->getFrameCount()) {
        currentFrame = 0;
    }
}

void SpriteComponent::setAnimationSpeed(float framesPerSecond) {
    animationSpeed = framesPerSecond;
}

void SpriteComponent::playAnimation(bool loop) {
    animating = true;
    looping = loop;
    animationTimer = 0.0f;
}

void SpriteComponent::stopAnimation() {
    animating = false;
    animationTimer = 0.0f;
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

