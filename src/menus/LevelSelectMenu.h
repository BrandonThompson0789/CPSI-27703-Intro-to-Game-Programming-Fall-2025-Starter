#ifndef LEVEL_SELECT_MENU_H
#define LEVEL_SELECT_MENU_H

#include "Menu.h"
#include <string>
#include <vector>
#include <SDL.h>
#include <SDL_ttf.h>

// Level information structure
struct LevelInfo {
    std::string id;           // Internal ID (e.g., "level1")
    std::string title;        // Display title (e.g., "Level 1: The Beginning")
    std::string filePath;     // Path to level JSON file (e.g., "assets/levels/level1.json")
    std::string thumbnailPath; // Path to thumbnail image (e.g., "assets/textures/level1.png")
    int order;                // Order for sorting (e.g., 10, 20, 30...)
    bool unlocked;            // Whether this level is unlocked
    
    // Cached resources for performance
    SDL_Texture* thumbnailTexture = nullptr;  // Cached thumbnail texture
    SDL_Texture* titleTexture = nullptr;      // Pre-rendered title text texture
    int titleTextureWidth = 0;
    int titleTextureHeight = 0;
};

class LevelSelectMenu : public Menu {
public:
    LevelSelectMenu(MenuManager* manager);
    virtual ~LevelSelectMenu();
    
    void update(float deltaTime) override;
    void onOpen() override;
    void handleNavigation(float upDown, float leftRight) override;
    void handleConfirm() override;
    void handleCancel() override;
    void handleMouse(int mouseX, int mouseY, bool mousePressed) override;
    
    // Custom rendering (override MenuManager's default rendering)
    bool render() override;
    
    // Update room code display (called when hosting state changes)
    void updateRoomCodeDisplay();
    
private:
    void setupLevels();
    void onPlay();
    void onContinue();
    void onRestart();
    void onBack();
    bool hasSaveDataForSelectedLevel() const;
    int getMaxButtonCount() const;
    
    // Resource management
    void loadCachedResources();
    void unloadCachedResources();
    void reloadLevelTextures();  // Reload textures when unlock status changes
    
    std::vector<LevelInfo> levels;
    int selectedLevelIndex;
    float scrollOffset;  // Horizontal scroll offset
    int selectedButtonIndex;  // 0 = Play/Continue, 1 = Restart (if shown), 2 = Back
    int hoveredButtonIndex;  // -1 = none, tracks which button mouse is hovering over
    bool inButtonMode;  // true when navigating buttons, false when navigating levels
    
    // Cached fonts (loaded once, reused)
    TTF_Font* titleFont = nullptr;
    TTF_Font* levelFont = nullptr;
    TTF_Font* buttonFont = nullptr;
    
    // Pre-rendered button text textures
    SDL_Texture* titleTextTexture = nullptr;  // Pre-rendered menu title
    SDL_Texture* playButtonTexture = nullptr;
    SDL_Texture* continueButtonTexture = nullptr;
    SDL_Texture* restartButtonTexture = nullptr;
    SDL_Texture* backButtonTexture = nullptr;
    SDL_Texture* lockedTextTexture = nullptr;
    SDL_Texture* roomCodeTexture = nullptr;
    SDL_Texture* copyButtonTexture = nullptr;
    int titleTextWidth = 0;
    int titleTextHeight = 0;
    int playButtonTextWidth = 0;
    int continueButtonTextWidth = 0;
    int restartButtonTextWidth = 0;
    int backButtonTextWidth = 0;
    int lockedTextWidth = 0;
    int lockedTextHeight = 0;
    int roomCodeTextureWidth = 0;
    int roomCodeTextureHeight = 0;
    int copyButtonTextureWidth = 0;
    int copyButtonTextureHeight = 0;
    
    // UI constants
    static constexpr int THUMBNAIL_WIDTH = 200;
    static constexpr int THUMBNAIL_HEIGHT = 150;
    static constexpr int LEVEL_SPACING = 20;
    static constexpr int LEVEL_PANEL_WIDTH = 250;
    static constexpr int BUTTON_HEIGHT = 40;
    static constexpr float SCROLL_SPEED = 500.0f;
};

#endif // LEVEL_SELECT_MENU_H

