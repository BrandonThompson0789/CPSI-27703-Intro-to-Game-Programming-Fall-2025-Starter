#include "SpriteManager.h"
#include <SDL_image.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

SpriteManager& SpriteManager::getInstance() {
    static SpriteManager instance;
    return instance;
}

SpriteManager::~SpriteManager() {
    cleanup();
}

void SpriteManager::init(SDL_Renderer* rendererParam, const std::string& spriteDataPath) {
    renderer = rendererParam;
    basePath = "assets/textures/";  // Default base path for textures
    
    if (!spriteDataPath.empty()) {
        loadSpriteData(spriteDataPath);
    }
}

bool SpriteManager::loadSpriteData(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "SpriteManager: Could not open sprite data file: " << filepath << std::endl;
        return false;
    }

    nlohmann::json j;
    try {
        file >> j;
        file.close();
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "SpriteManager: JSON parsing error: " << e.what() << std::endl;
        file.close();
        return false;
    }

    // Parse sprite data
    if (!j.contains("textures")) {
        std::cerr << "SpriteManager: No 'textures' field in sprite data" << std::endl;
        return false;
    }

    // Iterate through textures
    for (auto& [textureName, textureData] : j["textures"].items()) {
        if (!textureData.contains("sprites")) {
            continue;
        }

        // Iterate through sprites in this texture
        for (auto& [spriteName, spriteInfo] : textureData["sprites"].items()) {
            SpriteData data;
            data.textureName = textureName;

            // Check if sprite has multiple frames
            if (spriteInfo.contains("frames") && spriteInfo["frames"].is_array()) {
                // Load multiple frames
                for (const auto& frame : spriteInfo["frames"]) {
                    SpriteFrame sf;
                    sf.x = frame.value("x", 0);
                    sf.y = frame.value("y", 0);
                    sf.w = frame.value("w", 0);
                    sf.h = frame.value("h", 0);
                    data.frames.push_back(sf);
                }
            } else {
                // Single frame sprite
                SpriteFrame sf;
                sf.x = spriteInfo.value("x", 0);
                sf.y = spriteInfo.value("y", 0);
                sf.w = spriteInfo.value("w", 0);
                sf.h = spriteInfo.value("h", 0);
                data.frames.push_back(sf);
            }

            sprites[spriteName] = data;
        }
    }

    std::cout << "SpriteManager: Loaded " << sprites.size() << " sprites from " << filepath << std::endl;
    return true;
}

const SpriteData* SpriteManager::getSpriteData(const std::string& spriteName) const {
    auto it = sprites.find(spriteName);
    if (it != sprites.end()) {
        return &it->second;
    }
    return nullptr;
}

SDL_Texture* SpriteManager::getTexture(const std::string& textureName) {
    // Check if texture is already loaded
    auto it = textures.find(textureName);
    if (it != textures.end()) {
        return it->second;
    }

    // Load the texture
    SDL_Texture* texture = loadTexture(basePath + textureName);
    if (texture) {
        textures[textureName] = texture;
    }
    return texture;
}

void SpriteManager::renderSprite(const std::string& spriteName, int frame, float x, float y, 
                                 float angle, SDL_RendererFlip flip, uint8_t alpha,
                                 uint8_t colorR, uint8_t colorG, uint8_t colorB) {
    if (!renderer) {
        std::cerr << "SpriteManager: Renderer not initialized" << std::endl;
        return;
    }

    const SpriteData* spriteData = getSpriteData(spriteName);
    if (!spriteData) {
        std::cerr << "SpriteManager: Sprite not found: " << spriteName << std::endl;
        return;
    }

    SDL_Texture* texture = getTexture(spriteData->textureName);
    if (!texture) {
        std::cerr << "SpriteManager: Texture not found: " << spriteData->textureName << std::endl;
        return;
    }

    SpriteFrame frameData = spriteData->getFrame(frame);
    
    SDL_Rect srcRect = { frameData.x, frameData.y, frameData.w, frameData.h };
    float width = static_cast<float>(frameData.w);
    float height = static_cast<float>(frameData.h);
    SDL_FRect dstRect = { x - width * 0.5f, y - height * 0.5f, width, height };

    // Set texture alpha and color mod
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_SetTextureColorMod(texture, colorR, colorG, colorB);

    // Render with rotation and flip (angle provided in degrees)
    SDL_RenderCopyExF(renderer, texture, &srcRect, &dstRect, angle, nullptr, flip);
}

void SpriteManager::renderSprite(const std::string& spriteName, int frame, float x, float y, 
                                 float width, float height, float angle, 
                                 SDL_RendererFlip flip, uint8_t alpha,
                                 uint8_t colorR, uint8_t colorG, uint8_t colorB) {
    if (!renderer) {
        std::cerr << "SpriteManager: Renderer not initialized" << std::endl;
        return;
    }

    const SpriteData* spriteData = getSpriteData(spriteName);
    if (!spriteData) {
        std::cerr << "SpriteManager: Sprite not found: " << spriteName << std::endl;
        return;
    }

    SDL_Texture* texture = getTexture(spriteData->textureName);
    if (!texture) {
        std::cerr << "SpriteManager: Texture not found: " << spriteData->textureName << std::endl;
        return;
    }

    SpriteFrame frameData = spriteData->getFrame(frame);
    
    SDL_Rect srcRect = { frameData.x, frameData.y, frameData.w, frameData.h };
    float clampedWidth = std::max(width, 1.0f);
    float clampedHeight = std::max(height, 1.0f);
    SDL_FRect dstRect = { x - clampedWidth * 0.5f, y - clampedHeight * 0.5f, clampedWidth, clampedHeight };

    // Set texture alpha and color mod
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_SetTextureColorMod(texture, colorR, colorG, colorB);

    // Render with rotation and flip (angle provided in degrees)
    SDL_RenderCopyExF(renderer, texture, &srcRect, &dstRect, angle, nullptr, flip);
}

void SpriteManager::renderSpriteTiled(const std::string& spriteName, int frame, float x, float y,
                                     float width, float height, float tileWidth, float tileHeight,
                                     float angle, SDL_RendererFlip flip, uint8_t alpha,
                                     uint8_t colorR, uint8_t colorG, uint8_t colorB) {
    if (!renderer) {
        std::cerr << "SpriteManager: Renderer not initialized" << std::endl;
        return;
    }

    const SpriteData* spriteData = getSpriteData(spriteName);
    if (!spriteData) {
        std::cerr << "SpriteManager: Sprite not found: " << spriteName << std::endl;
        return;
    }

    SDL_Texture* texture = getTexture(spriteData->textureName);
    if (!texture) {
        std::cerr << "SpriteManager: Texture not found: " << spriteData->textureName << std::endl;
        return;
    }

    SpriteFrame frameData = spriteData->getFrame(frame);
    
    // Use provided tile size, or default to frame size
    float actualTileWidth = (tileWidth > 0.0f) ? tileWidth : static_cast<float>(frameData.w);
    float actualTileHeight = (tileHeight > 0.0f) ? tileHeight : static_cast<float>(frameData.h);
    
    // Set texture alpha and color mod
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_SetTextureColorMod(texture, colorR, colorG, colorB);
    
    float clampedWidth = std::max(width, 1.0f);
    float clampedHeight = std::max(height, 1.0f);
    float clampedTileWidth = std::max(actualTileWidth, 1.0f);
    float clampedTileHeight = std::max(actualTileHeight, 1.0f);

    int tilesX = static_cast<int>(std::ceil(clampedWidth / clampedTileWidth));
    int tilesY = static_cast<int>(std::ceil(clampedHeight / clampedTileHeight));
    tilesX = std::max(tilesX, 1);
    tilesY = std::max(tilesY, 1);
    
    // Calculate top-left corner to center the entire tiled area
    float startX = x - clampedWidth * 0.5f;
    float startY = y - clampedHeight * 0.5f;
    
    SDL_Rect srcRect = { frameData.x, frameData.y, frameData.w, frameData.h };
    
    // Render each tile
    for (int tileY = 0; tileY < tilesY; ++tileY) {
        for (int tileX = 0; tileX < tilesX; ++tileX) {
            float dstX = startX + (static_cast<float>(tileX) * clampedTileWidth);
            float dstY = startY + (static_cast<float>(tileY) * clampedTileHeight);
            
            // Clip the last tile if it goes beyond the desired size
            float dstW = clampedTileWidth;
            float dstH = clampedTileHeight;
            
            if (dstX + dstW > startX + clampedWidth) {
                dstW = (startX + clampedWidth) - dstX;
            }
            if (dstY + dstH > startY + clampedHeight) {
                dstH = (startY + clampedHeight) - dstY;
            }
            
            SDL_FRect dstRect = { dstX, dstY, dstW, dstH };
            SDL_Rect tileSrcRect = srcRect;

            if (dstW < clampedTileWidth) {
                float widthRatio = dstW / clampedTileWidth;
                int adjustedWidth = static_cast<int>(std::round(static_cast<float>(frameData.w) * widthRatio));
                tileSrcRect.w = std::clamp(adjustedWidth, 1, frameData.w);
            }

            if (dstH < clampedTileHeight) {
                float heightRatio = dstH / clampedTileHeight;
                int adjustedHeight = static_cast<int>(std::round(static_cast<float>(frameData.h) * heightRatio));
                tileSrcRect.h = std::clamp(adjustedHeight, 1, frameData.h);
            }
            
            // For rotation, we'd need to rotate the entire tiled area as one
            // For now, tiled sprites don't support rotation (can be added if needed)
            if (angle != 0.0f) {
                std::cerr << "Warning: Tiled sprites don't currently support rotation" << std::endl;
            }
            
            SDL_RenderCopyExF(renderer, texture, &tileSrcRect, &dstRect, 0, nullptr, flip);
        }
    }
}

void SpriteManager::unloadTexture(const std::string& textureName) {
    auto it = textures.find(textureName);
    if (it != textures.end()) {
        SDL_DestroyTexture(it->second);
        textures.erase(it);
        std::cout << "SpriteManager: Unloaded texture: " << textureName << std::endl;
    }
}

void SpriteManager::unloadAll() {
    // Unload all textures
    for (auto& [name, texture] : textures) {
        SDL_DestroyTexture(texture);
    }
    textures.clear();
    
    // Clear sprite data
    sprites.clear();
    
    std::cout << "SpriteManager: All sprites and textures unloaded" << std::endl;
}

void SpriteManager::cleanup() {
    unloadAll();
    renderer = nullptr;
}

SDL_Texture* SpriteManager::loadTexture(const std::string& filepath) {
    SDL_Surface* surface = IMG_Load(filepath.c_str());
    if (!surface) {
        std::cerr << "SpriteManager: Failed to load image: " << filepath 
                  << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }

    // Convert surface to RGBA format if it doesn't have an alpha channel
    // This ensures proper transparency blending
    SDL_Surface* convertedSurface = nullptr;
    if (surface->format->format != SDL_PIXELFORMAT_RGBA32 && 
        surface->format->format != SDL_PIXELFORMAT_ARGB8888 &&
        surface->format->format != SDL_PIXELFORMAT_ABGR8888 &&
        surface->format->format != SDL_PIXELFORMAT_BGRA8888) {
        // Convert to RGBA32 format to ensure alpha channel support
        convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (!convertedSurface) {
            std::cerr << "SpriteManager: Failed to convert surface format: " << filepath 
                      << " - " << SDL_GetError() << std::endl;
            return nullptr;
        }
        surface = convertedSurface;
    }

    SDL_Texture* texturePtr = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texturePtr) {
        std::cerr << "SpriteManager: Failed to create texture from: " << filepath 
                  << " - " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Enable alpha blending mode for proper transparency support
    SDL_SetTextureBlendMode(texturePtr, SDL_BLENDMODE_BLEND);

    std::cout << "SpriteManager: Loaded texture: " << filepath << std::endl;
    return texturePtr;
}

