#ifndef LEVEL_SELECT_MENU_H
#define LEVEL_SELECT_MENU_H

#include "Menu.h"
#include <string>
#include <vector>
#include <map>
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
    
    // Select a level by its ID (e.g., "level1") or file path
    void selectLevelById(const std::string& levelId);
    
    // Set a level to be selected when the menu opens (called before onOpen)
    void setPendingLevelSelection(const std::string& levelId);
    
private:
    void setupLevels();
    void onPlay();
    void onContinue();
    void onRestart();
    void onBack();
    bool hasSaveDataForSelectedLevel() const;
    void updateCachedSaveData() const;  // Update cached save data check (expensive operation)
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
    std::string pendingLevelSelection;  // Level ID to select when menu opens
    std::map<SDL_Texture*, uint8_t> thumbnailAlphaCache;  // Cache per-thumbnail alpha to avoid unnecessary texture mod calls
    
    // Cached save data check result (expensive operation, cache per level select instance)
    mutable bool cachedHasSaveData = false;
    mutable int cachedHasSaveDataLevelIndex = -1;  // Track which level the cache is for
    
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
    SDL_Texture* roomCodeLabelTexture = nullptr;  // Cached "Room Code:" label
    SDL_Texture* copyButtonTexture = nullptr;
    int titleTextWidth = 0;
    int titleTextHeight = 0;
    int playButtonTextWidth = 0;
    int playButtonTextHeight = 0;  // Cached to avoid SDL_QueryTexture every frame
    int continueButtonTextWidth = 0;
    int continueButtonTextHeight = 0;  // Cached to avoid SDL_QueryTexture every frame
    int restartButtonTextWidth = 0;
    int restartButtonTextHeight = 0;  // Cached to avoid SDL_QueryTexture every frame
    int backButtonTextWidth = 0;
    int backButtonTextHeight = 0;  // Cached to avoid SDL_QueryTexture every frame
    int lockedTextWidth = 0;
    int lockedTextHeight = 0;
    int roomCodeTextureWidth = 0;
    int roomCodeTextureHeight = 0;
    int roomCodeLabelWidth = 0;
    int roomCodeLabelHeight = 0;
    int copyButtonTextureWidth = 0;
    int copyButtonTextureHeight = 0;
    
    // Cached panel and button background textures for performance
    SDL_Texture* panelLockedTexture = nullptr;
    SDL_Texture* panelUnselectedTexture = nullptr;
    SDL_Texture* panelSelectedTexture = nullptr;
    SDL_Texture* panelButtonModeTexture = nullptr;
    SDL_Texture* buttonNormalTexture = nullptr;
    SDL_Texture* buttonContinueSelectedTexture = nullptr;
    SDL_Texture* buttonContinueNormalTexture = nullptr;
    SDL_Texture* buttonRestartSelectedTexture = nullptr;
    SDL_Texture* buttonRestartNormalTexture = nullptr;
    SDL_Texture* buttonPlaySelectedTexture = nullptr;
    SDL_Texture* buttonPlayNormalTexture = nullptr;
    SDL_Texture* buttonPlayDisabledTexture = nullptr;
    SDL_Texture* buttonBackSelectedTexture = nullptr;
    SDL_Texture* borderSelectedTexture = nullptr;
    SDL_Texture* borderButtonModeTexture = nullptr;
    
    // UI constants
    static constexpr int THUMBNAIL_WIDTH = 200;
    static constexpr int THUMBNAIL_HEIGHT = 150;
    static constexpr int LEVEL_SPACING = 20;
    static constexpr int LEVEL_PANEL_WIDTH = 250;
    static constexpr int BUTTON_HEIGHT = 40;
    static constexpr float SCROLL_SPEED = 500.0f;
};

#endif // LEVEL_SELECT_MENU_H

