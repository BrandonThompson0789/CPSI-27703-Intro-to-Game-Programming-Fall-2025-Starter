#include "level_editor/AssetCache.h"

#include <SDL.h>
#include <SDL_image.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace level_editor {

namespace fs = std::filesystem;

AssetCache::AssetCache() = default;

AssetCache::~AssetCache() {
    for (auto& kv : textures_) {
        if (kv.second) {
            SDL_DestroyTexture(kv.second);
        }
    }
    textures_.clear();
}

void AssetCache::Initialize(SDL_Renderer* renderer,
                            const std::string& spriteDataPath,
                            const std::string& texturesBasePath) {
    renderer_ = renderer;
    texturesBasePath_ = texturesBasePath;
    LoadSpriteData(spriteDataPath);
}

bool AssetCache::LoadSpriteData(const std::string& spriteDataPath) {
    sprites_.clear();
    std::ifstream stream(spriteDataPath);
    if (!stream.is_open()) {
        std::cerr << "[AssetCache] Failed to open sprite data: " << spriteDataPath << "\n";
        return false;
    }
    nlohmann::json data;
    stream >> data;

    const auto texturesIt = data.find("textures");
    if (texturesIt == data.end() || !texturesIt->is_object()) {
        return false;
    }

    for (const auto& textureEntry : texturesIt->items()) {
        const auto& textureName = textureEntry.key();
        const auto& textureData = textureEntry.value();
        const auto spritesIt = textureData.find("sprites");
        if (spritesIt == textureData.end() || !spritesIt->is_object()) {
            continue;
        }
        for (const auto& spriteEntry : spritesIt->items()) {
            SpriteInfo info;
            info.name = spriteEntry.key();
            const auto& spriteData = spriteEntry.value();
            SpriteFrameInfo frameInfo;
            frameInfo.textureName = textureName;
            frameInfo.x = spriteData.value("x", 0);
            frameInfo.y = spriteData.value("y", 0);
            frameInfo.w = spriteData.value("w", 0);
            frameInfo.h = spriteData.value("h", 0);
            info.frames.push_back(frameInfo);
            if (spriteData.contains("frames") && spriteData["frames"].is_array()) {
                info.frames.clear();
                for (const auto& frame : spriteData["frames"]) {
                    SpriteFrameInfo f;
                    f.textureName = textureName;
                    f.x = frame.value("x", 0);
                    f.y = frame.value("y", 0);
                    f.w = frame.value("w", 0);
                    f.h = frame.value("h", 0);
                    info.frames.push_back(f);
                }
            }
            sprites_[info.name] = info;
        }
    }

    return true;
}

SDL_Texture* AssetCache::LoadTexture(const std::string& textureName) {
    if (!renderer_) {
        return nullptr;
    }
    const fs::path texturePath = fs::path(texturesBasePath_) / textureName;
    SDL_Texture* texture = IMG_LoadTexture(renderer_, texturePath.string().c_str());
    if (!texture) {
        std::cerr << "[AssetCache] Failed to load texture: " << texturePath << " (" << IMG_GetError() << ")\n";
        return nullptr;
    }
    textures_[textureName] = texture;
    return texture;
}

SDL_Texture* AssetCache::GetTexture(const std::string& textureName) {
    auto it = textures_.find(textureName);
    if (it != textures_.end()) {
        return it->second;
    }
    return LoadTexture(textureName);
}

const SpriteInfo* AssetCache::GetSpriteInfo(const std::string& spriteName) const {
    auto it = sprites_.find(spriteName);
    if (it == sprites_.end()) {
        return nullptr;
    }
    return &it->second;
}

SpritePreview AssetCache::GetSpriteThumb(const std::string& spriteName) const {
    SpritePreview preview;
    preview.spriteName = spriteName;
    const auto* info = GetSpriteInfo(spriteName);
    if (info && !info->frames.empty()) {
        preview.width = info->frames[0].w;
        preview.height = info->frames[0].h;
    }
    return preview;
}

}  // namespace level_editor

