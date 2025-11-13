#pragma once

#include <SDL.h>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

class Engine;

// Represents a single background layer
struct BackgroundLayer {
    std::string spriteName;      // Sprite to use for this layer
    int frame = 0;                // Frame index (default 0)
    float parallaxX = 1.0f;       // Parallax factor for X (1.0 = no parallax, 0.0 = fixed)
    float parallaxY = 1.0f;       // Parallax factor for Y (1.0 = no parallax, 0.0 = fixed)
    bool tiled = false;           // Whether to tile the sprite
    float tileWidth = 0.0f;       // Tile width (0 = use sprite width)
    float tileHeight = 0.0f;      // Tile height (0 = use sprite height)
    float offsetX = 0.0f;         // World space X offset
    float offsetY = 0.0f;         // World space Y offset
    float width = 0.0f;           // Layer width (0 = use sprite width, or full view if tiled)
    float height = 0.0f;          // Layer height (0 = use sprite height, or full view if tiled)
    uint8_t alpha = 255;          // Alpha transparency (0-255)
    float depth = 0.0f;           // Depth/Z-order (lower = further back, rendered first)
    
    // Load from JSON
    void fromJson(const nlohmann::json& json);
};

// Manages background rendering with multiple layers, parallax, and tiling
class BackgroundManager {
public:
    BackgroundManager();
    ~BackgroundManager();
    
    // Initialize with renderer
    void init(SDL_Renderer* renderer);
    
    // Load background configuration from JSON
    void loadFromJson(const nlohmann::json& json, Engine* engine);
    
    // Clear all background layers
    void clear();
    
    // Render all background layers (should be called before rendering objects)
    void render(Engine* engine);
    
    // Cleanup
    void cleanup();
    
private:
    SDL_Renderer* renderer;
    std::vector<BackgroundLayer> layers;
    
    // Helper to render a single layer
    void renderLayer(const BackgroundLayer& layer, Engine* engine);
};

