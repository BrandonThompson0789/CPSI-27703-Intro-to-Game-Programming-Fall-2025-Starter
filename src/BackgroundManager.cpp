#include "BackgroundManager.h"
#include "Engine.h"
#include "SpriteManager.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <climits>

BackgroundManager::BackgroundManager() : renderer(nullptr) {
}

BackgroundManager::~BackgroundManager() {
    cleanup();
}

void BackgroundManager::init(SDL_Renderer* rendererParam) {
    renderer = rendererParam;
}

void BackgroundManager::loadFromJson(const nlohmann::json& json, Engine* engine) {
    clear();
    
    if (!json.contains("background") || !json["background"].is_object()) {
        // No background configuration, that's fine
        return;
    }
    
    const auto& bgJson = json["background"];
    
    // Check if it's a single layer or multiple layers
    if (bgJson.contains("layers") && bgJson["layers"].is_array()) {
        // Multiple layers
        for (const auto& layerJson : bgJson["layers"]) {
            BackgroundLayer layer;
            layer.fromJson(layerJson);
            layers.push_back(layer);
        }
    } else {
        // Single layer (backward compatibility)
        BackgroundLayer layer;
        layer.fromJson(bgJson);
        layers.push_back(layer);
    }
    
    // Sort layers by depth (lower depth = further back = render first)
    std::sort(layers.begin(), layers.end(), 
        [](const BackgroundLayer& a, const BackgroundLayer& b) {
            return a.depth < b.depth;
        });
    
    std::cout << "BackgroundManager: Loaded " << layers.size() << " background layer(s)" << std::endl;
}

void BackgroundManager::clear() {
    layers.clear();
}

void BackgroundManager::render(Engine* engine) {
    if (!renderer || !engine || layers.empty()) {
        return;
    }
    
    const auto& cameraState = engine->getCameraState();
    
    // Render each layer from back to front
    for (const auto& layer : layers) {
        renderLayer(layer, engine);
    }
}

void BackgroundManager::renderLayer(const BackgroundLayer& layer, Engine* engine) {
    if (!renderer || !engine) {
        return;
    }
    
    const SpriteData* spriteData = SpriteManager::getInstance().getSpriteData(layer.spriteName);
    if (!spriteData) {
        std::cerr << "BackgroundManager: Sprite not found: " << layer.spriteName << std::endl;
        return;
    }
    
    const auto& cameraState = engine->getCameraState();
    float cameraScale = cameraState.scale;
    
    // Calculate camera center position
    float cameraCenterX = cameraState.viewMinX + (cameraState.viewWidth * 0.5f);
    float cameraCenterY = cameraState.viewMinY + (cameraState.viewHeight * 0.5f);
    
    // Apply parallax: layer moves relative to camera movement
    // parallax = 1.0: moves with camera (normal, foreground)
    // parallax = 0.5: moves at half speed (midground)  
    // parallax = 0.0: fixed (background, doesn't move)
    // The layer's world position = offset + cameraCenter * parallax
    // This means when camera moves from C1 to C2, layer moves from (offset + C1*P) to (offset + C2*P)
    // So layer movement = (C2 - C1) * P, which is P times the camera movement
    float layerWorldCenterX = layer.offsetX + (cameraCenterX * layer.parallaxX);
    float layerWorldCenterY = layer.offsetY + (cameraCenterY * layer.parallaxY);
    
    // Get sprite frame data
    SpriteFrame frameData = spriteData->getFrame(layer.frame);
    float spriteWidth = static_cast<float>(frameData.w);
    float spriteHeight = static_cast<float>(frameData.h);
    
    // Determine actual tile size
    float actualTileWidth = (layer.tileWidth > 0.0f) ? layer.tileWidth : spriteWidth;
    float actualTileHeight = (layer.tileHeight > 0.0f) ? layer.tileHeight : spriteHeight;
    
    // Determine layer dimensions
    float layerWidth = layer.width;
    float layerHeight = layer.height;
    
    if (layer.tiled) {
        // For tiled layers, default to covering the entire view
        if (layerWidth <= 0.0f) {
            layerWidth = cameraState.viewWidth / layer.parallaxX;
        }
        if (layerHeight <= 0.0f) {
            layerHeight = cameraState.viewHeight / layer.parallaxY;
        }
    } else {
        // For non-tiled layers, default to sprite size
        if (layerWidth <= 0.0f) {
            layerWidth = spriteWidth;
        }
        if (layerHeight <= 0.0f) {
            layerHeight = spriteHeight;
        }
    }
    
    // Calculate world position (center of layer)
    float worldX = layerWorldCenterX;
    float worldY = layerWorldCenterY;
    
    // Convert to screen coordinates
    SDL_FPoint screenPos = engine->worldToScreen(worldX, worldY);
    float screenWidth = layerWidth * cameraScale;
    float screenHeight = layerHeight * cameraScale;
    
    if (layer.tiled) {
        // Render tiled background
        float worldTileWidth = actualTileWidth;
        float worldTileHeight = actualTileHeight;
        float screenTileWidth = worldTileWidth * cameraScale;
        float screenTileHeight = worldTileHeight * cameraScale;
        
        // For tiled backgrounds with parallax, calculate which tiles are visible
        // The parallax effect: when camera is at C, layer appears at offset + C*P in world space
        // Tiles are positioned in world space relative to the layer's parallax position
        
        // Calculate what world space area we need to cover
        // For a layer with parallax P, we need to render tiles that cover the camera view
        // but positioned according to the parallax offset
        
        // The camera sees world coordinates [viewMinX, viewMinX + viewWidth]
        // The layer's world position is: offset + (cameraCenter * parallax)
        // So we need tiles that cover this area, but accounting for parallax
        
        // Calculate the world space bounds we need to cover
        // For parallax layers, we need more tiles because the layer moves slower
        float worldViewMinX, worldViewMaxX, worldViewMinY, worldViewMaxY;
        
        // Calculate which world space area we need to cover
        // The camera sees [viewMinX, viewMinX + viewWidth] in world space
        // For parallax layers, we need to ensure tiles cover this area
        // Tiles are positioned at: layerWorldCenter + (tileOffset)
        // So we need tiles where (layerWorldCenter + tileOffset) covers [viewMinX, viewMinX + viewWidth]
        
        // Simply use the camera view bounds - tiles positioned with parallax will naturally cover it
        worldViewMinX = cameraState.viewMinX;
        worldViewMaxX = cameraState.viewMinX + cameraState.viewWidth;
        worldViewMinY = cameraState.viewMinY;
        worldViewMaxY = cameraState.viewMinY + cameraState.viewHeight;
        
        // Calculate tile range in world space
        // Tiles are positioned at: layerWorldCenter + (tileIndex * tileSize)
        // So we need tiles where: layerWorldCenter + (tileIndex * tileSize) is in [worldViewMin, worldViewMax]
        // tileIndex = (worldPos - layerWorldCenter) / tileSize
        
        float tileStartWorldX = std::floor((worldViewMinX - layerWorldCenterX) / worldTileWidth) * worldTileWidth + layerWorldCenterX;
        float tileStartWorldY = std::floor((worldViewMinY - layerWorldCenterY) / worldTileHeight) * worldTileHeight + layerWorldCenterY;
        float tileEndWorldX = std::ceil((worldViewMaxX - layerWorldCenterX) / worldTileWidth) * worldTileWidth + layerWorldCenterX;
        float tileEndWorldY = std::ceil((worldViewMaxY - layerWorldCenterY) / worldTileHeight) * worldTileHeight + layerWorldCenterY;
        
        // Calculate how many tiles to render
        int tilesX = static_cast<int>(std::ceil((tileEndWorldX - tileStartWorldX) / worldTileWidth));
        int tilesY = static_cast<int>(std::ceil((tileEndWorldY - tileStartWorldY) / worldTileHeight));
        
        // Clamp to reasonable limits to prevent performance issues
        tilesX = std::min(tilesX, 100);
        tilesY = std::min(tilesY, 100);
        
        // Render each tile
        SDL_Texture* texture = SpriteManager::getInstance().getTexture(spriteData->textureName);
        if (!texture) {
            return;
        }
        
        SDL_Rect srcRect = { frameData.x, frameData.y, frameData.w, frameData.h };
        SDL_SetTextureAlphaMod(texture, layer.alpha);
        
        for (int tileY = 0; tileY < tilesY; ++tileY) {
            for (int tileX = 0; tileX < tilesX; ++tileX) {
                // Calculate tile position directly in world space
                // Tiles are positioned relative to the layer's world center
                float tileWorldX = tileStartWorldX + (static_cast<float>(tileX) * worldTileWidth);
                float tileWorldY = tileStartWorldY + (static_cast<float>(tileY) * worldTileHeight);
                
                // Convert tile top-left corner to screen space
                SDL_FPoint tileScreenPos = engine->worldToScreen(tileWorldX, tileWorldY);
                
                // Only render if tile is visible on screen
                if (tileScreenPos.x + screenTileWidth >= 0 && 
                    tileScreenPos.x <= static_cast<float>(Engine::screenWidth) &&
                    tileScreenPos.y + screenTileHeight >= 0 && 
                    tileScreenPos.y <= static_cast<float>(Engine::screenHeight)) {
                    
                    // Render tile with top-left corner at screen position
                    SDL_FRect dstRect = {
                        tileScreenPos.x,
                        tileScreenPos.y,
                        screenTileWidth,
                        screenTileHeight
                    };
                    
                    SDL_RenderCopyExF(renderer, texture, &srcRect, &dstRect, 0.0f, nullptr, SDL_FLIP_NONE);
                }
            }
        }
    } else {
        // Render single sprite (stretched or at natural size)
        SDL_Texture* texture = SpriteManager::getInstance().getTexture(spriteData->textureName);
        if (!texture) {
            return;
        }
        
        SDL_Rect srcRect = { frameData.x, frameData.y, frameData.w, frameData.h };
        SDL_SetTextureAlphaMod(texture, layer.alpha);
        
        SDL_FRect dstRect = {
            screenPos.x - screenWidth * 0.5f,
            screenPos.y - screenHeight * 0.5f,
            screenWidth,
            screenHeight
        };
        
        SDL_RenderCopyExF(renderer, texture, &srcRect, &dstRect, 0.0f, nullptr, SDL_FLIP_NONE);
    }
}

void BackgroundManager::cleanup() {
    layers.clear();
    renderer = nullptr;
}

nlohmann::json BackgroundManager::toJson() const {
    nlohmann::json j = nlohmann::json::array();
    
    for (const auto& layer : layers) {
        nlohmann::json layerJson;
        layerJson["spriteName"] = layer.spriteName;
        layerJson["frame"] = layer.frame;
        layerJson["parallaxX"] = layer.parallaxX;
        layerJson["parallaxY"] = layer.parallaxY;
        layerJson["tiled"] = layer.tiled;
        layerJson["tileWidth"] = layer.tileWidth;
        layerJson["tileHeight"] = layer.tileHeight;
        layerJson["offsetX"] = layer.offsetX;
        layerJson["offsetY"] = layer.offsetY;
        layerJson["width"] = layer.width;
        layerJson["height"] = layer.height;
        layerJson["alpha"] = layer.alpha;
        layerJson["depth"] = layer.depth;
        j.push_back(layerJson);
    }
    
    return j;
}

void BackgroundLayer::fromJson(const nlohmann::json& json) {
    if (json.contains("spriteName") && json["spriteName"].is_string()) {
        spriteName = json["spriteName"].get<std::string>();
    }
    
    if (json.contains("frame") && json["frame"].is_number_integer()) {
        frame = json["frame"].get<int>();
    }
    
    if (json.contains("parallaxX") && json["parallaxX"].is_number()) {
        parallaxX = json["parallaxX"].get<float>();
    }
    
    if (json.contains("parallaxY") && json["parallaxY"].is_number()) {
        parallaxY = json["parallaxY"].get<float>();
    }
    
    if (json.contains("tiled") && json["tiled"].is_boolean()) {
        tiled = json["tiled"].get<bool>();
    }
    
    if (json.contains("tileWidth") && json["tileWidth"].is_number()) {
        tileWidth = json["tileWidth"].get<float>();
    }
    
    if (json.contains("tileHeight") && json["tileHeight"].is_number()) {
        tileHeight = json["tileHeight"].get<float>();
    }
    
    if (json.contains("offsetX") && json["offsetX"].is_number()) {
        offsetX = json["offsetX"].get<float>();
    }
    
    if (json.contains("offsetY") && json["offsetY"].is_number()) {
        offsetY = json["offsetY"].get<float>();
    }
    
    if (json.contains("width") && json["width"].is_number()) {
        width = json["width"].get<float>();
    }
    
    if (json.contains("height") && json["height"].is_number()) {
        height = json["height"].get<float>();
    }
    
    if (json.contains("alpha") && json["alpha"].is_number_integer()) {
        int alphaValue = json["alpha"].get<int>();
        alpha = static_cast<uint8_t>(std::clamp(alphaValue, 0, 255));
    }
    
    if (json.contains("depth") && json["depth"].is_number()) {
        depth = json["depth"].get<float>();
    }
}

