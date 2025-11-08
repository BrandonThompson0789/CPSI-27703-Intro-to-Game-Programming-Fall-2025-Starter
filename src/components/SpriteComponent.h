#pragma once
#include "Component.h"
#include <string>
#include <SDL.h>
#include <nlohmann/json.hpp>

class SpriteComponent : public Component {
public:
    SpriteComponent(Object& parent, const std::string& spriteName, bool animate = false, bool loop = false, int killAfterLoops = -1);
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
    void setKillAfterLoops(int loops);
    void clearKillAfterLoops();
    int getKillAfterLoops() const { return killAfterLoops; }
    int getCompletedLoops() const { return completedLoops; }
    void setRandomizeAnglePerFrame(bool enable);
    bool isRandomizingAnglePerFrame() const { return randomizeAnglePerFrame; }
    
    // Rendering options
    void setFlipHorizontal(bool flip);
    void setFlipVertical(bool flip);
    void setAlpha(uint8_t alpha);
    
    // Position management (for objects without BodyComponent)
    void setPosition(float x, float y, float angle = 0.0f);
    std::tuple<float, float, float> getPosition() const;
    void setAngle(float angle);
    float getAngle() const;
    
    // Tiling options
    void setTiled(bool tiled, float tileWidth = 0.0f, float tileHeight = 0.0f);
    void setRenderSize(float width, float height);

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
    
    // Local position (used when no BodyComponent exists)
    float localX{0.0f};
    float localY{0.0f};
    float localAngle{0.0f};
    
    // Tiling support
    bool tiled{false};
    float tileWidth{0.0f};   // Source tile size (0 = use sprite size)
    float tileHeight{0.0f};
    float renderWidth{0.0f};  // Custom render size (0 = use sprite size)
    float renderHeight{0.0f};

    // Animation lifecycle
    int killAfterLoops;   // -1 disables auto-death
    int completedLoops;
    bool randomizeAnglePerFrame;
    
    void applyRandomAngle();
};

