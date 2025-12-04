#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace level_editor {

struct SpritePreview {
    std::string spriteName;
    int width{0};
    int height{0};
};

struct SpriteFrameInfo {
    std::string textureName;
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

struct SpriteInfo {
    std::string name;
    std::vector<SpriteFrameInfo> frames;
};

class AssetCache {
public:
    AssetCache();
    ~AssetCache();

    void Initialize(SDL_Renderer* renderer,
                    const std::string& spriteDataPath,
                    const std::string& texturesBasePath);

    SDL_Texture* GetTexture(const std::string& textureName);
    const SpriteInfo* GetSpriteInfo(const std::string& spriteName) const;
    SpritePreview GetSpriteThumb(const std::string& spriteName) const;
    bool IsInitialized() const { return renderer_ != nullptr; }

private:
    bool LoadSpriteData(const std::string& spriteDataPath);
    SDL_Texture* LoadTexture(const std::string& textureName);

    SDL_Renderer* renderer_{nullptr};
    std::string texturesBasePath_;
    std::unordered_map<std::string, SpriteInfo> sprites_;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};

}  // namespace level_editor

