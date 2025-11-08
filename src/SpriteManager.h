#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <SDL.h>
#include <nlohmann/json.hpp>

// Represents a single frame of a sprite
struct SpriteFrame {
    int x, y, w, h;
};

// Represents sprite data with multiple frames
struct SpriteData {
    std::string textureName;
    std::vector<SpriteFrame> frames;
    
    // Default frame (first frame)
    SpriteFrame getFrame(int index = 0) const {
        if (frames.empty()) {
            return {0, 0, 0, 0};
        }
        if (index < 0 || index >= static_cast<int>(frames.size())) {
            return frames[0];
        }
        return frames[index];
    }
    
    int getFrameCount() const {
        return static_cast<int>(frames.size());
    }
};

// Singleton class to manage sprite data and textures
class SpriteManager {
public:
    // Get singleton instance
    static SpriteManager& getInstance();
    
    // Initialize with renderer and load sprite data
    void init(SDL_Renderer* renderer, const std::string& spriteDataPath = "assets/spriteData.json");
    
    // Load sprite data from JSON file
    bool loadSpriteData(const std::string& filepath);
    
    // Get sprite data by name
    const SpriteData* getSpriteData(const std::string& spriteName) const;
    
    // Get texture by name
    SDL_Texture* getTexture(const std::string& textureName);
    
    // Render a sprite frame (angle in degrees, centered on position)
    void renderSprite(const std::string& spriteName, int frame, float x, float y, 
                     float angle = 0.0f, SDL_RendererFlip flip = SDL_FLIP_NONE, 
                     uint8_t alpha = 255);
    
    // Render a sprite frame with custom width and height (angle in degrees)
    void renderSprite(const std::string& spriteName, int frame, float x, float y, 
                     float width, float height, float angle = 0.0f, 
                     SDL_RendererFlip flip = SDL_FLIP_NONE, uint8_t alpha = 255);
    
    // Render a sprite tiled (repeated) instead of stretched
    void renderSpriteTiled(const std::string& spriteName, int frame, float x, float y,
                          float width, float height, float tileWidth, float tileHeight,
                          float angle = 0.0f, SDL_RendererFlip flip = SDL_FLIP_NONE,
                          uint8_t alpha = 255);
    
    // Unload specific texture
    void unloadTexture(const std::string& textureName);
    
    // Unload all textures and sprite data
    void unloadAll();
    
    // Cleanup
    void cleanup();
    
private:
    SpriteManager() = default;
    ~SpriteManager();
    
    // Delete copy constructor and assignment operator
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;
    
    SDL_Renderer* renderer = nullptr;
    std::unordered_map<std::string, SpriteData> sprites;
    std::unordered_map<std::string, SDL_Texture*> textures;
    std::string basePath;  // Base path for texture files
    
    // Helper to load a texture from file
    SDL_Texture* loadTexture(const std::string& filepath);
};

