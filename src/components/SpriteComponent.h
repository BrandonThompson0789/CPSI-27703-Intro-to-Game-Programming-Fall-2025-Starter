#pragma once
#include "Component.h"
#include <string>
#include <SDL.h>

class SpriteComponent : public Component {
public:
    SpriteComponent(Object& parent, const std::string& spriteName);
    ~SpriteComponent() override = default;

    void update() override;
    void draw() override;

    // Animation control
    void setFrame(int frameIndex);
    void setAnimationSpeed(float framesPerSecond);
    void playAnimation(bool loop = true);
    void stopAnimation();
    
    // Rendering options
    void setFlipHorizontal(bool flip);
    void setFlipVertical(bool flip);
    void setAlpha(uint8_t alpha);

private:
    std::string spriteName;
    int currentFrame;
    bool animating;
    bool looping;
    float animationSpeed;  // frames per second
    float animationTimer;
    
    // Rendering flags
    SDL_RendererFlip flipFlags;
    uint8_t alpha;
};

