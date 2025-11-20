#include "LevelSelectMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../HostManager.h"
#include "../SaveManager.h"
#include "../SpriteManager.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

LevelSelectMenu::LevelSelectMenu(MenuManager* manager)
    : Menu(manager), selectedLevelIndex(-1), scrollOffset(0.0f),
      selectedButtonIndex(0), hoveredButtonIndex(-1), inButtonMode(false) {
    setTitle("Select Level");
    setupLevels();
    loadCachedResources();
}

LevelSelectMenu::~LevelSelectMenu() {
    unloadCachedResources();
}

void LevelSelectMenu::setupLevels() {
    levels.clear();
    
    // Scan assets/levels directory for JSON files
    std::string levelsDir = "assets/levels";
    
    try {
        if (std::filesystem::exists(levelsDir) && std::filesystem::is_directory(levelsDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(levelsDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filePath = entry.path().string();
                    std::string fileName = entry.path().stem().string();
                    
                    // Exclude level_mainmenu from being counted as a level
                    if (fileName == "level_mainmenu") {
                        continue;
                    }
                    
                    // Load level metadata from JSON
                    std::ifstream file(filePath);
                    if (!file.is_open()) {
                        std::cerr << "LevelSelectMenu: Could not open level file: " << filePath << std::endl;
                        continue;
                    }
                    
                    nlohmann::json levelJson;
                    try {
                        file >> levelJson;
                        file.close();
                    } catch (const nlohmann::json::exception& e) {
                        std::cerr << "LevelSelectMenu: JSON parsing error for " << filePath << ": " << e.what() << std::endl;
                        file.close();
                        continue;
                    }
                    
                    LevelInfo level;
                    level.id = fileName;
                    level.filePath = filePath;
                    
                    // Get title (default to filename if not provided)
                    if (levelJson.contains("title") && levelJson["title"].is_string()) {
                        level.title = levelJson["title"].get<std::string>();
                    } else {
                        level.title = fileName;
                    }
                    
                    // Get order (default to 0 if not provided)
                    if (levelJson.contains("order") && levelJson["order"].is_number_integer()) {
                        level.order = levelJson["order"].get<int>();
                    } else {
                        level.order = 0;
                    }
                    
                    // Get thumbnail (default to level_default.png if not provided)
                    if (levelJson.contains("thumbnail") && levelJson["thumbnail"].is_string()) {
                        level.thumbnailPath = levelJson["thumbnail"].get<std::string>();
                    } else {
                        level.thumbnailPath = "assets/textures/level_default.png";
                    }
                    
                    // First level (lowest order) is always unlocked
                    // Others are locked by default
                    level.unlocked = false;
                    
                    levels.push_back(level);
                }
            }
        } else {
            std::cerr << "LevelSelectMenu: Levels directory not found: " << levelsDir << std::endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "LevelSelectMenu: Error scanning levels directory: " << e.what() << std::endl;
    }
    
    // Sort levels by order
    std::sort(levels.begin(), levels.end(), [](const LevelInfo& a, const LevelInfo& b) {
        return a.order < b.order;
    });
    
    // Determine which levels are unlocked based on progression
    SaveManager& saveMgr = SaveManager::getInstance();
    int progression = saveMgr.getLevelProgression();
    
    // Levels with order <= progression are complete
    // Level with order == progression + 10 is playable (next level)
    // Levels with order > progression + 10 are locked
    for (auto& level : levels) {
        if (level.order <= progression) {
            // Level is complete (unlocked and playable)
            level.unlocked = true;
        } else if (level.order == progression + 10) {
            // Next level is playable but not complete
            level.unlocked = true;
        } else {
            // Level is locked
            level.unlocked = false;
        }
    }
    
    // Select first level by default (even if locked)
    if (!levels.empty()) {
        selectedLevelIndex = 0;
    }
}

void LevelSelectMenu::onOpen() {
    Menu::onOpen();
    selectedLevelIndex = -1;
    scrollOffset = 0.0f;
    selectedButtonIndex = 0;
    hoveredButtonIndex = -1;
    inButtonMode = false;
    
    // Reset cached save data check
    cachedHasSaveData = false;
    cachedHasSaveDataLevelIndex = -1;
    
    // Update room code display if hosting
    updateRoomCodeDisplay();
    
    // Refresh level unlock status (in case progression changed)
    SaveManager& saveMgr = SaveManager::getInstance();
    int progression = saveMgr.getLevelProgression();
    
    bool unlockStatusChanged = false;
    for (auto& level : levels) {
        bool wasUnlocked = level.unlocked;
        if (level.order <= progression) {
            level.unlocked = true;
        } else if (level.order == progression + 10) {
            level.unlocked = true;
        } else {
            level.unlocked = false;
        }
        if (wasUnlocked != level.unlocked) {
            unlockStatusChanged = true;
        }
    }
    
    // Reload textures if unlock status changed (thumbnails may need to change)
    if (unlockStatusChanged) {
        reloadLevelTextures();
    }
    
    // Select first level by default (even if locked)
    if (!levels.empty()) {
        selectedLevelIndex = 0;
        // Cache save data check for the initially selected level
        updateCachedSaveData();
    }
    
    // Calculate initial scroll offset to center selected level
    if (selectedLevelIndex >= 0) {
        int screenWidth = Engine::screenWidth;
        scrollOffset = static_cast<float>(selectedLevelIndex) * (LEVEL_PANEL_WIDTH + LEVEL_SPACING) 
                       - (screenWidth - LEVEL_PANEL_WIDTH) * 0.5f;
    }
}

void LevelSelectMenu::update(float deltaTime) {
    Menu::update(deltaTime);
    
    // Smooth scroll to selected level
    // The selected panel will be centered, so calculate offset to center it
    if (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size())) {
        int screenWidth = Engine::screenWidth;
        // Calculate what the scroll offset should be to center the selected panel
        // Panel X position = -scrollOffset + index * (width + spacing)
        // For selected panel to be centered: panelX = (screenWidth - LEVEL_PANEL_WIDTH) / 2
        // So: -scrollOffset + selectedIndex * (width + spacing) = (screenWidth - LEVEL_PANEL_WIDTH) / 2
        // Therefore: scrollOffset = selectedIndex * (width + spacing) - (screenWidth - LEVEL_PANEL_WIDTH) / 2
        float targetOffset = static_cast<float>(selectedLevelIndex) * (LEVEL_PANEL_WIDTH + LEVEL_SPACING) 
                            - (screenWidth - LEVEL_PANEL_WIDTH) * 0.5f;
        
        float diff = targetOffset - scrollOffset;
        if (std::abs(diff) > 1.0f) {
            float scrollAmount = SCROLL_SPEED * deltaTime;
            if (diff > 0) {
                scrollOffset = std::min(scrollOffset + scrollAmount, targetOffset);
            } else {
                scrollOffset = std::max(scrollOffset - scrollAmount, targetOffset);
            }
        } else {
            scrollOffset = targetOffset;
        }
    }
}

void LevelSelectMenu::handleNavigation(float upDown, float leftRight) {
    if (levels.empty()) return;
    
    mouseMode = false;
    hoveredButtonIndex = -1;  // Clear hover when using keyboard/gamepad
    
    // Handle vertical navigation (up/down) - switch between level selection and button selection
    if (upDown > 0.5f) {
        // Down pressed - enter button mode if not already
        if (!inButtonMode && selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size()) 
            && levels[selectedLevelIndex].unlocked) {
            inButtonMode = true;
            selectedButtonIndex = 0;
        } else if (inButtonMode) {
            // Already in button mode, cycle through buttons
            int maxButtons = getMaxButtonCount();
            selectedButtonIndex = (selectedButtonIndex + 1) % maxButtons;
        }
    } else if (upDown < -0.5f) {
        // Up pressed - exit button mode if in it
        if (inButtonMode) {
            inButtonMode = false;
            selectedButtonIndex = 0;
        }
    }
    
    // Handle horizontal navigation (left/right)
    if (!inButtonMode) {
        // Navigating levels - can select any level (including locked ones)
        int oldSelectedIndex = selectedLevelIndex;
        if (leftRight > 0.5f) {
            // Right pressed - select next level
            selectedLevelIndex = (selectedLevelIndex + 1) % static_cast<int>(levels.size());
        } else if (leftRight < -0.5f) {
            // Left pressed - select previous level
            selectedLevelIndex = (selectedLevelIndex - 1 + static_cast<int>(levels.size())) % static_cast<int>(levels.size());
        }
        // Update cached save data if level selection changed
        if (oldSelectedIndex != selectedLevelIndex) {
            updateCachedSaveData();
        }
    } else {
        // Navigating buttons
        if (leftRight > 0.5f) {
            // Right pressed - move to next button
            int maxButtons = getMaxButtonCount();
            selectedButtonIndex = (selectedButtonIndex + 1) % maxButtons;
        } else if (leftRight < -0.5f) {
            // Left pressed - move to previous button
            int maxButtons = getMaxButtonCount();
            selectedButtonIndex = (selectedButtonIndex - 1 + maxButtons) % maxButtons;
        }
    }
}

void LevelSelectMenu::handleConfirm() {
    if (inButtonMode) {
        // Activate selected button
        int maxButtons = getMaxButtonCount();
        if (selectedButtonIndex >= 0 && selectedButtonIndex < maxButtons) {
            SaveManager& saveMgr = SaveManager::getInstance();
            bool hasSaveForLevel = hasSaveDataForSelectedLevel();
            bool isUnlocked = (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size()) 
                              && levels[selectedLevelIndex].unlocked);
            
            if (hasSaveForLevel) {
                // Continue, Restart, Back buttons
                if (selectedButtonIndex == 0) {
                    onContinue();
                } else if (selectedButtonIndex == 1) {
                    onRestart();
                } else if (selectedButtonIndex == 2) {
                    onBack();
                }
            } else {
                // Play, Back buttons
                if (selectedButtonIndex == 0 && isUnlocked) {
                    // Only allow Play if level is unlocked
                    onPlay();
                } else if (selectedButtonIndex == 1 || (selectedButtonIndex == 0 && !isUnlocked)) {
                    // Back button (or Play if locked, which does nothing)
                    onBack();
                }
            }
        }
    } else {
        // Enter button mode (can enter even for locked levels, but Play will be disabled)
        if (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size())) {
            inButtonMode = true;
            selectedButtonIndex = 0;
        }
    }
}

void LevelSelectMenu::handleCancel() {
    onBack();
}

void LevelSelectMenu::handleMouse(int mouseX, int mouseY, bool mousePressed) {
    mouseMode = true;
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    // Check if mouse clicked on copy button
    Engine* engine = menuManager->getEngine();
    if (engine && engine->isHosting() && copyButtonTexture) {
        HostManager* hostMgr = engine->getHostManager();
        if (hostMgr && roomCodeTexture) {
            int roomCodeX = 60;  // Moved 50 pixels to the right
            int roomCodeY = 10;
            int copyX = roomCodeX + 160 + roomCodeTextureWidth + 10;
            int copyWidth = copyButtonTextureWidth + 10;
            int copyHeight = copyButtonTextureHeight + 5;
            
            if (mouseX >= copyX && mouseX < copyX + copyWidth &&
                mouseY >= roomCodeY && mouseY < roomCodeY + copyHeight && mousePressed) {
                std::string roomCode = hostMgr->GetRoomCode();
                if (!roomCode.empty()) {
                    SDL_SetClipboardText(roomCode.c_str());
                }
                return;  // Don't process other mouse events
            }
        }
    }
    
    // Check if mouse is over a level panel
    int levelAreaY = screenHeight / 2 - 100;
    int levelAreaHeight = THUMBNAIL_HEIGHT + 60;
    
    if (mouseY >= levelAreaY && mouseY < levelAreaY + levelAreaHeight) {
        // Calculate which level panel the mouse is over
        float panelStartX = -scrollOffset;
        for (size_t i = 0; i < levels.size(); ++i) {
            int panelX = static_cast<int>(panelStartX + i * (LEVEL_PANEL_WIDTH + LEVEL_SPACING));
            int panelCenterX = panelX + LEVEL_PANEL_WIDTH / 2;
            
            if (mouseX >= panelX && mouseX < panelX + LEVEL_PANEL_WIDTH) {
                // Only select level on click, not on hover
                // Clicking level panel only highlights it, doesn't enter button mode
                if (mousePressed) {
                    int oldSelectedIndex = selectedLevelIndex;
                    selectedLevelIndex = static_cast<int>(i);
                    // Exit button mode when selecting a level
                    inButtonMode = false;
                    selectedButtonIndex = 0;
                    // Update cached save data if level selection changed
                    if (oldSelectedIndex != selectedLevelIndex) {
                        updateCachedSaveData();
                    }
                }
                break;
            }
        }
    }
    
    // Check if mouse is over buttons
    int buttonAreaY = screenHeight - 100;  // Match render position
    int buttonWidth = 150;
    int buttonSpacing = 20;
    
    bool hasSaveForLevel = hasSaveDataForSelectedLevel();
    bool isUnlocked = (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size()) 
                      && levels[selectedLevelIndex].unlocked);
    
    // Reset hover state
    hoveredButtonIndex = -1;
    
    if (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size()) 
        && mouseY >= buttonAreaY && mouseY < buttonAreaY + BUTTON_HEIGHT) {
        
        if (hasSaveForLevel && isUnlocked) {
            // Continue, Restart, Back buttons
            int buttonStartX = screenWidth / 2 - buttonWidth - buttonSpacing / 2;
            
            // Continue button
            if (mouseX >= buttonStartX && mouseX < buttonStartX + buttonWidth) {
                hoveredButtonIndex = 0;
                if (mousePressed) {
                    onContinue();
                }
            }
            // Restart button
            else if (mouseX >= screenWidth / 2 + buttonSpacing / 2 && 
                     mouseX < screenWidth / 2 + buttonSpacing / 2 + buttonWidth) {
                hoveredButtonIndex = 1;
                if (mousePressed) {
                    onRestart();
                }
            }
            // Back button
            else if (mouseX >= screenWidth / 2 + buttonWidth + buttonSpacing * 2 && 
                     mouseX < screenWidth / 2 + buttonWidth * 2 + buttonSpacing * 2) {
                hoveredButtonIndex = 2;
                if (mousePressed) {
                    onBack();
                }
            }
        } else {
            // Play, Back buttons (Play is disabled if level is locked)
            int playX = screenWidth / 2 - buttonWidth / 2;
            
            // Play button (only works if unlocked)
            if (mouseX >= playX && mouseX < playX + buttonWidth) {
                hoveredButtonIndex = 0;
                if (mousePressed && isUnlocked) {
                    onPlay();
                }
            }
            // Back button
            else if (mouseX >= screenWidth / 2 + buttonWidth + buttonSpacing && 
                     mouseX < screenWidth / 2 + buttonWidth * 2 + buttonSpacing) {
                hoveredButtonIndex = 1;
                if (mousePressed) {
                    onBack();
                }
            }
        }
    }
}

void LevelSelectMenu::loadCachedResources() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer) {
        return;
    }
    
    // Load fonts once
    if (TTF_WasInit()) {
        titleFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 36);
        levelFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 20);
        buttonFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 24);
    }
    
    // Pre-render title texture
    if (titleFont) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* titleSurface = TTF_RenderUTF8_Blended(titleFont, getTitle().c_str(), white);
        if (titleSurface) {
            titleTextTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
            titleTextWidth = titleSurface->w;
            titleTextHeight = titleSurface->h;
            SDL_FreeSurface(titleSurface);
        }
    }
    
    // Pre-render button text textures
    if (buttonFont) {
        SDL_Color white = {255, 255, 255, 255};
        
        // Play button
        SDL_Surface* playSurface = TTF_RenderUTF8_Blended(buttonFont, "Play", white);
        if (playSurface) {
            playButtonTexture = SDL_CreateTextureFromSurface(renderer, playSurface);
            playButtonTextWidth = playSurface->w;
            playButtonTextHeight = playSurface->h;
            SDL_FreeSurface(playSurface);
        }
        
        // Continue button
        SDL_Surface* continueSurface = TTF_RenderUTF8_Blended(buttonFont, "Continue", white);
        if (continueSurface) {
            continueButtonTexture = SDL_CreateTextureFromSurface(renderer, continueSurface);
            continueButtonTextWidth = continueSurface->w;
            continueButtonTextHeight = continueSurface->h;
            SDL_FreeSurface(continueSurface);
        }
        
        // Restart button
        SDL_Surface* restartSurface = TTF_RenderUTF8_Blended(buttonFont, "Restart", white);
        if (restartSurface) {
            restartButtonTexture = SDL_CreateTextureFromSurface(renderer, restartSurface);
            restartButtonTextWidth = restartSurface->w;
            restartButtonTextHeight = restartSurface->h;
            SDL_FreeSurface(restartSurface);
        }
        
        // Back button
        SDL_Surface* backSurface = TTF_RenderUTF8_Blended(buttonFont, "Back", white);
        if (backSurface) {
            backButtonTexture = SDL_CreateTextureFromSurface(renderer, backSurface);
            backButtonTextWidth = backSurface->w;
            backButtonTextHeight = backSurface->h;
            SDL_FreeSurface(backSurface);
        }
    }
    
    // Pre-render "LOCKED" text
    if (levelFont) {
        SDL_Color lockColor = {200, 200, 200, 255};
        SDL_Surface* lockedSurface = TTF_RenderUTF8_Blended(levelFont, "LOCKED", lockColor);
        if (lockedSurface) {
            lockedTextTexture = SDL_CreateTextureFromSurface(renderer, lockedSurface);
            lockedTextWidth = lockedSurface->w;
            lockedTextHeight = lockedSurface->h;
            SDL_FreeSurface(lockedSurface);
        }
    }
    
    // Pre-render "Room Code:" label
    if (buttonFont) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* labelSurface = TTF_RenderUTF8_Blended(buttonFont, "Room Code:", white);
        if (labelSurface) {
            roomCodeLabelTexture = SDL_CreateTextureFromSurface(renderer, labelSurface);
            roomCodeLabelWidth = labelSurface->w;
            roomCodeLabelHeight = labelSurface->h;
            SDL_FreeSurface(labelSurface);
        }
    }
    
    // Pre-render panel backgrounds as textures for performance
    int panelWidth = LEVEL_PANEL_WIDTH;
    int panelHeight = THUMBNAIL_HEIGHT + 60;
    SDL_Texture* oldTarget = SDL_GetRenderTarget(renderer);
    
    // Locked panel (dark gray)
    panelLockedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panelWidth, panelHeight);
    if (panelLockedTexture) {
        SDL_SetRenderTarget(renderer, panelLockedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        SDL_Rect panelRect = {0, 0, panelWidth, panelHeight};
        SDL_RenderFillRect(renderer, &panelRect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &panelRect);
    }
    
    // Unselected panel (darker gray)
    panelUnselectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panelWidth, panelHeight);
    if (panelUnselectedTexture) {
        SDL_SetRenderTarget(renderer, panelUnselectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect panelRect = {0, 0, panelWidth, panelHeight};
        SDL_RenderFillRect(renderer, &panelRect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &panelRect);
    }
    
    // Selected panel (light blue, thicker white border)
    panelSelectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panelWidth, panelHeight);
    if (panelSelectedTexture) {
        SDL_SetRenderTarget(renderer, panelSelectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
        SDL_Rect panelRect = {0, 0, panelWidth, panelHeight};
        SDL_RenderFillRect(renderer, &panelRect);
        // Draw thick white border (3 pixels)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int t = 0; t < 3; ++t) {
            SDL_Rect borderRect = {-t, -t, panelWidth + t * 2, panelHeight + t * 2};
            SDL_RenderDrawRect(renderer, &borderRect);
        }
    }
    
    // Button mode panel (dimmer blue, thinner border)
    panelButtonModeTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, panelWidth, panelHeight);
    if (panelButtonModeTexture) {
        SDL_SetRenderTarget(renderer, panelButtonModeTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 70, 100, 130, 255);
        SDL_Rect panelRect = {0, 0, panelWidth, panelHeight};
        SDL_RenderFillRect(renderer, &panelRect);
        // Draw medium border (2 pixels)
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        for (int t = 0; t < 2; ++t) {
            SDL_Rect borderRect = {-t, -t, panelWidth + t * 2, panelHeight + t * 2};
            SDL_RenderDrawRect(renderer, &borderRect);
        }
    }
    
    // Pre-render button backgrounds as textures
    int buttonWidth = 150;
    int buttonHeight = BUTTON_HEIGHT;
    
    // Normal button (unselected) - using gray for back button normal state
    buttonNormalTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonNormalTexture) {
        SDL_SetRenderTarget(renderer, buttonNormalTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Continue button selected (green)
    buttonContinueSelectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonContinueSelectedTexture) {
        SDL_SetRenderTarget(renderer, buttonContinueSelectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 80, 200, 80, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Continue button normal (darker green)
    buttonContinueNormalTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonContinueNormalTexture) {
        SDL_SetRenderTarget(renderer, buttonContinueNormalTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Restart button selected (orange)
    buttonRestartSelectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonRestartSelectedTexture) {
        SDL_SetRenderTarget(renderer, buttonRestartSelectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Restart button normal (darker orange)
    buttonRestartNormalTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonRestartNormalTexture) {
        SDL_SetRenderTarget(renderer, buttonRestartNormalTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 150, 100, 50, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Play button selected (green)
    buttonPlaySelectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonPlaySelectedTexture) {
        SDL_SetRenderTarget(renderer, buttonPlaySelectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 80, 200, 80, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Play button normal (darker green)
    buttonPlayNormalTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonPlayNormalTexture) {
        SDL_SetRenderTarget(renderer, buttonPlayNormalTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Play button disabled (dark gray)
    buttonPlayDisabledTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonPlayDisabledTexture) {
        SDL_SetRenderTarget(renderer, buttonPlayDisabledTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    // Back button selected (light gray)
    buttonBackSelectedTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, buttonWidth, buttonHeight);
    if (buttonBackSelectedTexture) {
        SDL_SetRenderTarget(renderer, buttonBackSelectedTexture);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_Rect buttonRect = {0, 0, buttonWidth, buttonHeight};
        SDL_RenderFillRect(renderer, &buttonRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
    }
    
    SDL_SetRenderTarget(renderer, oldTarget);
    
    // Load level textures
    reloadLevelTextures();
    
    // Load room code resources if hosting
    updateRoomCodeDisplay();
}

void LevelSelectMenu::unloadCachedResources() {
    // Unload fonts
    if (titleFont) {
        TTF_CloseFont(titleFont);
        titleFont = nullptr;
    }
    if (levelFont) {
        TTF_CloseFont(levelFont);
        levelFont = nullptr;
    }
    if (buttonFont) {
        TTF_CloseFont(buttonFont);
        buttonFont = nullptr;
    }
    
    // Unload title texture
    if (titleTextTexture) {
        SDL_DestroyTexture(titleTextTexture);
        titleTextTexture = nullptr;
    }
    
    // Unload button textures
    if (playButtonTexture) {
        SDL_DestroyTexture(playButtonTexture);
        playButtonTexture = nullptr;
    }
    if (continueButtonTexture) {
        SDL_DestroyTexture(continueButtonTexture);
        continueButtonTexture = nullptr;
    }
    if (restartButtonTexture) {
        SDL_DestroyTexture(restartButtonTexture);
        restartButtonTexture = nullptr;
    }
    if (backButtonTexture) {
        SDL_DestroyTexture(backButtonTexture);
        backButtonTexture = nullptr;
    }
    if (lockedTextTexture) {
        SDL_DestroyTexture(lockedTextTexture);
        lockedTextTexture = nullptr;
    }
    if (roomCodeTexture) {
        SDL_DestroyTexture(roomCodeTexture);
        roomCodeTexture = nullptr;
    }
    if (roomCodeLabelTexture) {
        SDL_DestroyTexture(roomCodeLabelTexture);
        roomCodeLabelTexture = nullptr;
    }
    if (copyButtonTexture) {
        SDL_DestroyTexture(copyButtonTexture);
        copyButtonTexture = nullptr;
    }
    
    // Unload panel textures
    if (panelLockedTexture) {
        SDL_DestroyTexture(panelLockedTexture);
        panelLockedTexture = nullptr;
    }
    if (panelUnselectedTexture) {
        SDL_DestroyTexture(panelUnselectedTexture);
        panelUnselectedTexture = nullptr;
    }
    if (panelSelectedTexture) {
        SDL_DestroyTexture(panelSelectedTexture);
        panelSelectedTexture = nullptr;
    }
    if (panelButtonModeTexture) {
        SDL_DestroyTexture(panelButtonModeTexture);
        panelButtonModeTexture = nullptr;
    }
    
    // Unload button textures
    if (buttonNormalTexture) {
        SDL_DestroyTexture(buttonNormalTexture);
        buttonNormalTexture = nullptr;
    }
    if (buttonContinueSelectedTexture) {
        SDL_DestroyTexture(buttonContinueSelectedTexture);
        buttonContinueSelectedTexture = nullptr;
    }
    if (buttonContinueNormalTexture) {
        SDL_DestroyTexture(buttonContinueNormalTexture);
        buttonContinueNormalTexture = nullptr;
    }
    if (buttonRestartSelectedTexture) {
        SDL_DestroyTexture(buttonRestartSelectedTexture);
        buttonRestartSelectedTexture = nullptr;
    }
    if (buttonRestartNormalTexture) {
        SDL_DestroyTexture(buttonRestartNormalTexture);
        buttonRestartNormalTexture = nullptr;
    }
    if (buttonPlaySelectedTexture) {
        SDL_DestroyTexture(buttonPlaySelectedTexture);
        buttonPlaySelectedTexture = nullptr;
    }
    if (buttonPlayNormalTexture) {
        SDL_DestroyTexture(buttonPlayNormalTexture);
        buttonPlayNormalTexture = nullptr;
    }
    if (buttonPlayDisabledTexture) {
        SDL_DestroyTexture(buttonPlayDisabledTexture);
        buttonPlayDisabledTexture = nullptr;
    }
    if (buttonBackSelectedTexture) {
        SDL_DestroyTexture(buttonBackSelectedTexture);
        buttonBackSelectedTexture = nullptr;
    }
    
    // Unload level textures
    for (auto& level : levels) {
        if (level.thumbnailTexture) {
            SDL_DestroyTexture(level.thumbnailTexture);
            level.thumbnailTexture = nullptr;
        }
        if (level.titleTexture) {
            SDL_DestroyTexture(level.titleTexture);
            level.titleTexture = nullptr;
        }
    }
}

void LevelSelectMenu::reloadLevelTextures() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer) {
        return;
    }
    
    // Unload existing level textures
    for (auto& level : levels) {
        if (level.thumbnailTexture) {
            SDL_DestroyTexture(level.thumbnailTexture);
            level.thumbnailTexture = nullptr;
        }
        if (level.titleTexture) {
            SDL_DestroyTexture(level.titleTexture);
            level.titleTexture = nullptr;
        }
    }
    
    // Load thumbnails and pre-render title textures
    for (auto& level : levels) {
        // Load thumbnail texture
        if (level.unlocked) {
            // Try to load thumbnail from specified path
            SDL_Surface* thumbSurface = IMG_Load(level.thumbnailPath.c_str());
            if (thumbSurface) {
                level.thumbnailTexture = SDL_CreateTextureFromSurface(renderer, thumbSurface);
                SDL_FreeSurface(thumbSurface);
            }
            
            // If specified thumbnail failed, try default
            if (!level.thumbnailTexture && level.thumbnailPath != "assets/textures/level_default.png") {
                SDL_Surface* defaultSurface = IMG_Load("assets/textures/level_default.png");
                if (defaultSurface) {
                    level.thumbnailTexture = SDL_CreateTextureFromSurface(renderer, defaultSurface);
                    SDL_FreeSurface(defaultSurface);
                }
            }
        } else {
            // Locked levels always use default thumbnail
            SDL_Surface* defaultSurface = IMG_Load("assets/textures/level_default.png");
            if (defaultSurface) {
                level.thumbnailTexture = SDL_CreateTextureFromSurface(renderer, defaultSurface);
                SDL_FreeSurface(defaultSurface);
            }
        }
        
        // Pre-render level title texture
        if (levelFont) {
            SDL_Color textColor = level.unlocked ? SDL_Color{255, 255, 255, 255} : SDL_Color{150, 150, 150, 255};
            SDL_Surface* textSurface = TTF_RenderUTF8_Blended(levelFont, level.title.c_str(), textColor);
            if (textSurface) {
                level.titleTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                level.titleTextureWidth = textSurface->w;
                level.titleTextureHeight = textSurface->h;
                SDL_FreeSurface(textSurface);
            }
        }
    }
}

bool LevelSelectMenu::render() {
    if (!menuManager || !menuManager->getEngine()) {
        return true;  // Indicate we handled rendering (even if nothing was drawn)
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer) {
        return true;  // Indicate we handled rendering
    }
    
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    // Draw semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 230);
    SDL_Rect bgRect = {0, 0, screenWidth, screenHeight};
    SDL_RenderFillRect(renderer, &bgRect);
    
    // Draw title (using pre-rendered texture)
    if (titleTextTexture) {
        SDL_Rect titleRect;
        titleRect.w = titleTextWidth;
        titleRect.h = titleTextHeight;
        titleRect.x = (screenWidth - titleRect.w) / 2;
        titleRect.y = 40;
        SDL_RenderCopy(renderer, titleTextTexture, nullptr, &titleRect);
    }
    
    // Draw room code if hosting
    Engine* engine = menuManager->getEngine();
    if (engine && engine->isHosting()) {
        HostManager* hostMgr = engine->getHostManager();
        if (hostMgr && roomCodeTexture) {
            int roomCodeY = 10;
            int roomCodeX = 60;  // Moved 50 pixels to the right
            
            // Draw room code label (using cached texture)
            if (roomCodeLabelTexture) {
                SDL_Rect labelRect;
                labelRect.w = roomCodeLabelWidth;
                labelRect.h = roomCodeLabelHeight;
                labelRect.x = roomCodeX;
                labelRect.y = roomCodeY;
                SDL_RenderCopy(renderer, roomCodeLabelTexture, nullptr, &labelRect);
            }
            
            // Draw room code
            SDL_Rect codeRect;
            codeRect.w = roomCodeTextureWidth;
            codeRect.h = roomCodeTextureHeight;
            codeRect.x = roomCodeX + 160;
            codeRect.y = roomCodeY;
            SDL_RenderCopy(renderer, roomCodeTexture, nullptr, &codeRect);
            
            // Draw copy button
            if (copyButtonTexture) {
                int copyX = roomCodeX + 160 + roomCodeTextureWidth + 10;
                int copyY = roomCodeY;
                int copyWidth = copyButtonTextureWidth;
                int copyHeight = copyButtonTextureHeight;
                
                SDL_Color copyColor = {100, 150, 200, 255};
                SDL_SetRenderDrawColor(renderer, copyColor.r, copyColor.g, copyColor.b, copyColor.a);
                SDL_Rect copyRect = {copyX, copyY, copyWidth + 10, copyHeight + 5};
                SDL_RenderFillRect(renderer, &copyRect);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &copyRect);
                
                SDL_Rect copyTextRect;
                copyTextRect.w = copyButtonTextureWidth;
                copyTextRect.h = copyButtonTextureHeight;
                copyTextRect.x = copyX + 5;
                copyTextRect.y = copyY + 2;
                SDL_RenderCopy(renderer, copyButtonTexture, nullptr, &copyTextRect);
            }
        }
    }
    
    // Draw level panels
    int levelAreaY = screenHeight / 2 - 100;
    float panelStartX = -scrollOffset;
    
    for (size_t i = 0; i < levels.size(); ++i) {
        const LevelInfo& level = levels[i];
        // Calculate panel position using scroll offset
        // This ensures the selected panel is centered (via scroll offset calculation)
        int panelX = static_cast<int>(panelStartX + i * (LEVEL_PANEL_WIDTH + LEVEL_SPACING));
        int panelCenterX = panelX + LEVEL_PANEL_WIDTH / 2;
        
        // Only draw if visible on screen
        if (panelX + LEVEL_PANEL_WIDTH >= 0 && panelX < screenWidth) {
            bool isSelected = (selectedLevelIndex >= 0 && static_cast<size_t>(selectedLevelIndex) == i);
            bool isUnlocked = level.unlocked;
            
            // Draw level panel background using cached texture
            SDL_Rect panelRect;
            panelRect.x = panelX;
            panelRect.y = levelAreaY;
            panelRect.w = LEVEL_PANEL_WIDTH;
            panelRect.h = THUMBNAIL_HEIGHT + 60;
            
            SDL_Texture* panelTexture = nullptr;
            if (!isUnlocked) {
                panelTexture = panelLockedTexture;
            } else if (isSelected && !inButtonMode) {
                panelTexture = panelSelectedTexture;
            } else if (isSelected && inButtonMode) {
                panelTexture = panelButtonModeTexture;
            } else {
                panelTexture = panelUnselectedTexture;
            }
            
            if (panelTexture) {
                SDL_RenderCopy(renderer, panelTexture, nullptr, &panelRect);
            } else {
                // Fallback to dynamic rendering if texture not available
                SDL_Color panelColor;
                if (!isUnlocked) {
                    panelColor = {60, 60, 60, 255};
                } else if (isSelected && !inButtonMode) {
                    panelColor = {100, 150, 200, 255};
                } else if (isSelected && inButtonMode) {
                    panelColor = {70, 100, 130, 255};
                } else {
                    panelColor = {50, 50, 50, 255};
                }
                SDL_SetRenderDrawColor(renderer, panelColor.r, panelColor.g, panelColor.b, panelColor.a);
                SDL_RenderFillRect(renderer, &panelRect);
                
                SDL_Color borderColor;
                int borderThickness = 1;
                if (isSelected && !inButtonMode) {
                    borderColor = SDL_Color{255, 255, 255, 255};
                    borderThickness = 3;
                } else if (isSelected && inButtonMode) {
                    borderColor = SDL_Color{180, 180, 180, 255};
                    borderThickness = 2;
                } else {
                    borderColor = SDL_Color{100, 100, 100, 255};
                }
                SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
                for (int t = 0; t < borderThickness; ++t) {
                    SDL_Rect borderRect = {panelRect.x - t, panelRect.y - t, panelRect.w + t * 2, panelRect.h + t * 2};
                    SDL_RenderDrawRect(renderer, &borderRect);
                }
            }
            
            // Draw thumbnail (using cached texture)
            int thumbX = panelCenterX;
            int thumbY = levelAreaY + 30;
            
            if (level.thumbnailTexture) {
                SDL_Rect thumbRect;
                thumbRect.w = THUMBNAIL_WIDTH;
                thumbRect.h = THUMBNAIL_HEIGHT;
                thumbRect.x = thumbX - thumbRect.w / 2;
                thumbRect.y = thumbY - thumbRect.h / 2;
                
                // Set alpha modulation for transparency (locked levels are semi-transparent)
                // Only set if it changed for this specific texture to avoid expensive texture mod calls
                uint8_t thumbnailAlpha = isUnlocked ? 255 : 128;
                auto it = thumbnailAlphaCache.find(level.thumbnailTexture);
                if (it == thumbnailAlphaCache.end() || it->second != thumbnailAlpha) {
                    SDL_SetTextureAlphaMod(level.thumbnailTexture, thumbnailAlpha);
                    thumbnailAlphaCache[level.thumbnailTexture] = thumbnailAlpha;
                }
                SDL_RenderCopy(renderer, level.thumbnailTexture, nullptr, &thumbRect);
            } else {
                // Draw placeholder if thumbnail not found
                SDL_Rect placeholderRect;
                placeholderRect.w = THUMBNAIL_WIDTH;
                placeholderRect.h = THUMBNAIL_HEIGHT;
                placeholderRect.x = thumbX - placeholderRect.w / 2;
                placeholderRect.y = thumbY - placeholderRect.h / 2;
                
                // Use semi-transparent gray for locked levels, normal gray for unlocked
                uint8_t placeholderAlpha = isUnlocked ? 255 : 128;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, placeholderAlpha);
                SDL_RenderFillRect(renderer, &placeholderRect);
            }
            
            // Draw level title (using pre-rendered texture)
            if (level.titleTexture) {
                SDL_Rect textRect;
                textRect.w = level.titleTextureWidth;
                textRect.h = level.titleTextureHeight;
                textRect.x = panelCenterX - textRect.w / 2;
                textRect.y = levelAreaY + THUMBNAIL_HEIGHT + 40;
                SDL_RenderCopy(renderer, level.titleTexture, nullptr, &textRect);
            }
            
            // Draw lock icon if locked (using pre-rendered texture)
            if (!isUnlocked && lockedTextTexture) {
                SDL_Rect lockRect;
                lockRect.w = lockedTextWidth;
                lockRect.h = lockedTextHeight;
                lockRect.x = thumbX - lockRect.w / 2;
                lockRect.y = thumbY - lockRect.h / 2;
                SDL_RenderCopy(renderer, lockedTextTexture, nullptr, &lockRect);
            }
        }
    }
    
    // Draw buttons (anchored near bottom of screen)
    int buttonAreaY = screenHeight - 100;  // 100 pixels from bottom
    int buttonWidth = 150;
    int buttonHeight = BUTTON_HEIGHT;
    int buttonSpacing = 20;
    
    bool hasSaveForLevel = hasSaveDataForSelectedLevel();
    bool isUnlocked = (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size()) 
                      && levels[selectedLevelIndex].unlocked);
    
    if (selectedLevelIndex >= 0 && selectedLevelIndex < static_cast<int>(levels.size())) {
        if (hasSaveForLevel && isUnlocked) {
            // Draw Continue and Restart buttons
            int buttonStartX = screenWidth / 2 - buttonWidth - buttonSpacing / 2;
            
            // Continue button
            bool continueSelected = (inButtonMode && selectedButtonIndex == 0) || (mouseMode && hoveredButtonIndex == 0);
            SDL_Rect continueRect = {buttonStartX, buttonAreaY, buttonWidth, buttonHeight};
            if (continueSelected && buttonContinueSelectedTexture) {
                SDL_RenderCopy(renderer, buttonContinueSelectedTexture, nullptr, &continueRect);
            } else if (buttonContinueNormalTexture) {
                SDL_RenderCopy(renderer, buttonContinueNormalTexture, nullptr, &continueRect);
            } else {
                // Fallback to dynamic rendering
                SDL_Color continueColor = continueSelected ? SDL_Color{80, 200, 80, 255} : SDL_Color{50, 150, 50, 255};
                SDL_SetRenderDrawColor(renderer, continueColor.r, continueColor.g, continueColor.b, continueColor.a);
                SDL_RenderFillRect(renderer, &continueRect);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &continueRect);
            }
            
            // Draw Continue button text (using pre-rendered texture and cached dimensions)
            if (continueButtonTexture && continueButtonTextWidth > 0 && continueButtonTextHeight > 0) {
                SDL_Rect textRect;
                textRect.w = continueButtonTextWidth;
                textRect.h = continueButtonTextHeight;
                textRect.x = buttonStartX + (buttonWidth - textRect.w) / 2;
                textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                SDL_RenderCopy(renderer, continueButtonTexture, nullptr, &textRect);
            }
            
            // Restart button
            int restartX = screenWidth / 2 + buttonSpacing / 2;
            bool restartSelected = (inButtonMode && selectedButtonIndex == 1) || (mouseMode && hoveredButtonIndex == 1);
            SDL_Rect restartRect = {restartX, buttonAreaY, buttonWidth, buttonHeight};
            if (restartSelected && buttonRestartSelectedTexture) {
                SDL_RenderCopy(renderer, buttonRestartSelectedTexture, nullptr, &restartRect);
            } else if (buttonRestartNormalTexture) {
                SDL_RenderCopy(renderer, buttonRestartNormalTexture, nullptr, &restartRect);
            } else {
                // Fallback to dynamic rendering
                SDL_Color restartColor = restartSelected ? SDL_Color{200, 150, 100, 255} : SDL_Color{150, 100, 50, 255};
                SDL_SetRenderDrawColor(renderer, restartColor.r, restartColor.g, restartColor.b, restartColor.a);
                SDL_RenderFillRect(renderer, &restartRect);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &restartRect);
            }
            
            // Draw Restart button text (using pre-rendered texture and cached dimensions)
            if (restartButtonTexture && restartButtonTextWidth > 0 && restartButtonTextHeight > 0) {
                SDL_Rect textRect;
                textRect.w = restartButtonTextWidth;
                textRect.h = restartButtonTextHeight;
                textRect.x = restartX + (buttonWidth - textRect.w) / 2;
                textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                SDL_RenderCopy(renderer, restartButtonTexture, nullptr, &textRect);
            }
        } else {
            // Draw Play button (disabled if level is locked)
            int playX = screenWidth / 2 - buttonWidth / 2;
            bool playSelected = (inButtonMode && selectedButtonIndex == 0) || (mouseMode && hoveredButtonIndex == 0 && isUnlocked);
            bool playDisabled = !isUnlocked;
            SDL_Rect playRect = {playX, buttonAreaY, buttonWidth, buttonHeight};
            if (playDisabled && buttonPlayDisabledTexture) {
                SDL_RenderCopy(renderer, buttonPlayDisabledTexture, nullptr, &playRect);
            } else if (playSelected && buttonPlaySelectedTexture) {
                SDL_RenderCopy(renderer, buttonPlaySelectedTexture, nullptr, &playRect);
            } else if (buttonPlayNormalTexture) {
                SDL_RenderCopy(renderer, buttonPlayNormalTexture, nullptr, &playRect);
            } else {
                // Fallback to dynamic rendering
                SDL_Color playColor = playSelected ? SDL_Color{80, 200, 80, 255} : SDL_Color{50, 150, 50, 255};
                SDL_SetRenderDrawColor(renderer, playColor.r, playColor.g, playColor.b, playColor.a);
                SDL_RenderFillRect(renderer, &playRect);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &playRect);
            }
            
            // Draw Play button text (using pre-rendered texture and cached dimensions)
            if (playButtonTexture && playButtonTextWidth > 0 && playButtonTextHeight > 0) {
                SDL_Rect textRect;
                textRect.w = playButtonTextWidth;
                textRect.h = playButtonTextHeight;
                textRect.x = playX + (buttonWidth - textRect.w) / 2;
                textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                
                // Apply alpha modulation for disabled state (only when necessary)
                if (playDisabled) {
                    SDL_SetTextureAlphaMod(playButtonTexture, 150);  // Dimmed for disabled
                }
                SDL_RenderCopy(renderer, playButtonTexture, nullptr, &textRect);
                // Reset alpha after rendering if it was modified
                if (playDisabled) {
                    SDL_SetTextureAlphaMod(playButtonTexture, 255);  // Reset to full opacity
                }
            }
        }
        
        // Draw Back button
        int backButtonIndex;
        if (hasSaveForLevel && isUnlocked) {
            backButtonIndex = 2;  // Continue, Restart, Back
        } else {
            backButtonIndex = 1;  // Play, Back (or Play disabled, Back)
        }
        int backX = (hasSaveForLevel && isUnlocked) ? screenWidth / 2 + buttonWidth + buttonSpacing * 2 : screenWidth / 2 + buttonWidth + buttonSpacing;
        bool backSelected = (inButtonMode && selectedButtonIndex == backButtonIndex) || (mouseMode && hoveredButtonIndex == backButtonIndex);
        SDL_Rect backRect = {backX, buttonAreaY, buttonWidth, buttonHeight};
        if (backSelected && buttonBackSelectedTexture) {
            SDL_RenderCopy(renderer, buttonBackSelectedTexture, nullptr, &backRect);
        } else if (buttonNormalTexture) {
            SDL_RenderCopy(renderer, buttonNormalTexture, nullptr, &backRect);
        } else {
            // Fallback to dynamic rendering
            SDL_Color backColor = backSelected ? SDL_Color{150, 150, 150, 255} : SDL_Color{100, 100, 100, 255};
            SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
            SDL_RenderFillRect(renderer, &backRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &backRect);
        }
        
        // Draw Back button text (using pre-rendered texture and cached dimensions)
        if (backButtonTexture && backButtonTextWidth > 0 && backButtonTextHeight > 0) {
            SDL_Rect textRect;
            textRect.w = backButtonTextWidth;
            textRect.h = backButtonTextHeight;
            textRect.x = backX + (buttonWidth - textRect.w) / 2;
            textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
            SDL_RenderCopy(renderer, backButtonTexture, nullptr, &textRect);
        }
    }
    
    return true;  // Indicate we handled rendering
}

void LevelSelectMenu::onPlay() {
    if (selectedLevelIndex < 0 || selectedLevelIndex >= static_cast<int>(levels.size())) {
        return;
    }
    
    const LevelInfo& level = levels[selectedLevelIndex];
    if (!level.unlocked) {
        return;
    }
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Load the level
    engine->loadFile(level.filePath);
    
    // Update SaveManager with current level
    SaveManager::getInstance().setCurrentLevel(level.id);
    
    // Close all menus to start the game
    menuManager->closeAllMenus();
}

void LevelSelectMenu::onContinue() {
    if (selectedLevelIndex < 0 || selectedLevelIndex >= static_cast<int>(levels.size())) {
        return;
    }
    
    const LevelInfo& level = levels[selectedLevelIndex];
    if (!level.unlocked) {
        return;
    }
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Load the save game - this loads the level data and starts the game
    engine->loadGame("save.json");
    std::cout << "LevelSelectMenu: Loaded save game for " << level.id << std::endl;
    
    // Close all menus to start the game
    menuManager->closeAllMenus();
}

void LevelSelectMenu::onRestart() {
    // Restart works the same as Play - loads the level file fresh
    onPlay();
}

void LevelSelectMenu::onBack() {
    if (menuManager) {
        menuManager->returnToMainMenu();
    }
}

void LevelSelectMenu::updateCachedSaveData() const {
    // Reset cache
    cachedHasSaveData = false;
    cachedHasSaveDataLevelIndex = -1;
    
    if (selectedLevelIndex < 0 || selectedLevelIndex >= static_cast<int>(levels.size())) {
        return;
    }
    
    const LevelInfo& level = levels[selectedLevelIndex];
    SaveManager& saveMgr = SaveManager::getInstance();
    
    // Check if save file exists
    if (!saveMgr.saveExists()) {
        return;
    }
    
    // Check if current level matches selected level
    std::string currentLevel = saveMgr.getCurrentLevel();
    if (currentLevel != level.id || currentLevel.empty()) {
        return;
    }
    
    // Check if save file has valid level data (background and objects populated)
    // This is the expensive operation - cache the result
    cachedHasSaveData = saveMgr.hasValidLevelData("save.json");
    cachedHasSaveDataLevelIndex = selectedLevelIndex;
}

bool LevelSelectMenu::hasSaveDataForSelectedLevel() const {
    // Use cached value if available and still valid for current selection
    if (cachedHasSaveDataLevelIndex == selectedLevelIndex) {
        return cachedHasSaveData;
    }
    
    // Cache is invalid or not set - update it
    updateCachedSaveData();
    return cachedHasSaveData;
}

int LevelSelectMenu::getMaxButtonCount() const {
    if (selectedLevelIndex < 0 || selectedLevelIndex >= static_cast<int>(levels.size())) {
        return 1;  // Just Back button
    }
    
    bool isUnlocked = levels[selectedLevelIndex].unlocked;
    bool hasSaveForLevel = hasSaveDataForSelectedLevel();
    
    if (hasSaveForLevel && isUnlocked) {
        // Continue, Restart, Back (3 buttons)
        return 3;
    } else if (isUnlocked) {
        // Play, Back (2 buttons)
        return 2;
    } else {
        // Locked level - Play (disabled), Back (2 buttons)
        return 2;
    }
}

void LevelSelectMenu::updateRoomCodeDisplay() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    if (!engine || !engine->isHosting()) {
        // Not hosting - clear room code textures
        if (roomCodeTexture) {
            SDL_DestroyTexture(roomCodeTexture);
            roomCodeTexture = nullptr;
        }
        if (copyButtonTexture) {
            SDL_DestroyTexture(copyButtonTexture);
            copyButtonTexture = nullptr;
        }
        return;
    }
    
    HostManager* hostMgr = engine->getHostManager();
    if (!hostMgr) {
        return;
    }
    
    std::string roomCode = hostMgr->GetRoomCode();
    if (roomCode.empty()) {
        return;
    }
    
    SDL_Renderer* renderer = engine->getRenderer();
    if (!renderer) {
        return;
    }
    
    // Create room code texture
    if (roomCodeTexture) {
        SDL_DestroyTexture(roomCodeTexture);
        roomCodeTexture = nullptr;
    }
    
    if (buttonFont) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* codeSurface = TTF_RenderUTF8_Blended(buttonFont, roomCode.c_str(), white);
        if (codeSurface) {
            roomCodeTexture = SDL_CreateTextureFromSurface(renderer, codeSurface);
            roomCodeTextureWidth = codeSurface->w;
            roomCodeTextureHeight = codeSurface->h;
            SDL_FreeSurface(codeSurface);
        }
    }
    
    // Create copy button texture
    if (copyButtonTexture) {
        SDL_DestroyTexture(copyButtonTexture);
        copyButtonTexture = nullptr;
    }
    
    if (buttonFont) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* copySurface = TTF_RenderUTF8_Blended(buttonFont, "Copy", white);
        if (copySurface) {
            copyButtonTexture = SDL_CreateTextureFromSurface(renderer, copySurface);
            copyButtonTextureWidth = copySurface->w;
            copyButtonTextureHeight = copySurface->h;
            SDL_FreeSurface(copySurface);
        }
    }
}

