#pragma once
#include "Component.h"
#include <string>
#include <SDL.h>
#include <nlohmann/json.hpp>

class SpriteComponent : public Component {
public:
    SpriteComponent(Object& parent, const std::string& spriteName, bool animate = false, bool loop = false);
    SpriteComponent(Object& parent, const nlohmann::json& data);
    ~SpriteComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "SpriteComponent"; }

    // Animation control
    void setCurrentSprite(const std::string& spriteName);
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

