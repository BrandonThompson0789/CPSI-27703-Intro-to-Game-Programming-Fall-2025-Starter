#include "SpriteManager.h"
#include <SDL_image.h>
#include <iostream>
#include <fstream>

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

void SpriteManager::renderSprite(const std::string& spriteName, int frame, int x, int y, 
                                 float angle, SDL_RendererFlip flip, uint8_t alpha) {
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
    SDL_Rect dstRect = { x, y, frameData.w, frameData.h };

    // Set texture alpha
    SDL_SetTextureAlphaMod(texture, alpha);

    // Render with rotation and flip
    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, angle, nullptr, flip);
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

    SDL_Texture* texturePtr = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texturePtr) {
        std::cerr << "SpriteManager: Failed to create texture from: " << filepath 
                  << " - " << SDL_GetError() << std::endl;
        return nullptr;
    }

    std::cout << "SpriteManager: Loaded texture: " << filepath << std::endl;
    return texturePtr;
}

