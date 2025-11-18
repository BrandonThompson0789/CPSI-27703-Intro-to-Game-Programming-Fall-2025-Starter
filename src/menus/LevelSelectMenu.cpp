#include "LevelSelectMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../SaveManager.h"
#include "../SpriteManager.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

LevelSelectMenu::LevelSelectMenu(MenuManager* manager)
    : Menu(manager), selectedLevelIndex(-1), scrollOffset(0.0f),
      selectedButtonIndex(0), hoveredButtonIndex(-1), inButtonMode(false) {
    setTitle("Select Level");
    setupLevels();
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
    
    // Refresh level unlock status (in case progression changed)
    SaveManager& saveMgr = SaveManager::getInstance();
    int progression = saveMgr.getLevelProgression();
    
    for (auto& level : levels) {
        if (level.order <= progression) {
            level.unlocked = true;
        } else if (level.order == progression + 10) {
            level.unlocked = true;
        } else {
            level.unlocked = false;
        }
    }
    
    // Select first level by default (even if locked)
    if (!levels.empty()) {
        selectedLevelIndex = 0;
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
        if (leftRight > 0.5f) {
            // Right pressed - select next level
            selectedLevelIndex = (selectedLevelIndex + 1) % static_cast<int>(levels.size());
        } else if (leftRight < -0.5f) {
            // Left pressed - select previous level
            selectedLevelIndex = (selectedLevelIndex - 1 + static_cast<int>(levels.size())) % static_cast<int>(levels.size());
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
                    selectedLevelIndex = static_cast<int>(i);
                    // Exit button mode when selecting a level
                    inButtonMode = false;
                    selectedButtonIndex = 0;
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
    
    // Load fonts
    TTF_Font* titleFont = nullptr;
    TTF_Font* levelFont = nullptr;
    TTF_Font* buttonFont = nullptr;
    
    if (TTF_WasInit()) {
        titleFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 36);
        levelFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 20);
        buttonFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 24);
    }
    
    // Draw title
    if (titleFont) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* titleSurface = TTF_RenderUTF8_Blended(titleFont, getTitle().c_str(), white);
        if (titleSurface) {
            SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
            if (titleTexture) {
                SDL_Rect titleRect;
                titleRect.w = titleSurface->w;
                titleRect.h = titleSurface->h;
                titleRect.x = (screenWidth - titleRect.w) / 2;
                titleRect.y = 40;
                SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);
                SDL_DestroyTexture(titleTexture);
            }
            SDL_FreeSurface(titleSurface);
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
            
            // Draw level panel background
            SDL_Color panelColor;
            if (!isUnlocked) {
                panelColor = {60, 60, 60, 255};  // Dark gray for locked
            } else if (isSelected && !inButtonMode) {
                panelColor = {100, 150, 200, 255};  // Light blue for selected level (not in button mode)
            } else if (isSelected && inButtonMode) {
                panelColor = {70, 100, 130, 255};  // Dimmer blue when in button mode
            } else {
                panelColor = {50, 50, 50, 255};  // Darker gray for unselected (more contrast)
            }
            
            SDL_Rect panelRect;
            panelRect.x = panelX;
            panelRect.y = levelAreaY;
            panelRect.w = LEVEL_PANEL_WIDTH;
            panelRect.h = THUMBNAIL_HEIGHT + 60;
            
            SDL_SetRenderDrawColor(renderer, panelColor.r, panelColor.g, panelColor.b, panelColor.a);
            SDL_RenderFillRect(renderer, &panelRect);
            
            // Draw border - thicker and brighter when selected and not in button mode
            SDL_Color borderColor;
            int borderThickness = 1;
            if (isSelected && !inButtonMode) {
                borderColor = SDL_Color{255, 255, 255, 255};  // Bright white when level selected
                borderThickness = 3;  // Thicker border
            } else if (isSelected && inButtonMode) {
                borderColor = SDL_Color{180, 180, 180, 255};  // Dimmer when button mode active
                borderThickness = 2;
            } else {
                borderColor = SDL_Color{100, 100, 100, 255};  // Dark gray for unselected
            }
            SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            // Draw thicker border by drawing multiple rectangles
            for (int t = 0; t < borderThickness; ++t) {
                SDL_Rect borderRect = {panelRect.x - t, panelRect.y - t, panelRect.w + t * 2, panelRect.h + t * 2};
                SDL_RenderDrawRect(renderer, &borderRect);
            }
            
            // Draw thumbnail
            int thumbX = panelCenterX;
            int thumbY = levelAreaY + 30;
            
            SDL_Texture* thumbnail = nullptr;
            bool thumbnailLoaded = false;
            uint8_t thumbnailAlpha = 255;  // Full opacity by default
            
            if (isUnlocked) {
                // Try to load thumbnail from specified path
                SDL_Surface* thumbSurface = IMG_Load(level.thumbnailPath.c_str());
                if (thumbSurface) {
                    thumbnail = SDL_CreateTextureFromSurface(renderer, thumbSurface);
                    SDL_FreeSurface(thumbSurface);
                    if (thumbnail) {
                        thumbnailLoaded = true;
                    }
                }
                
                // If specified thumbnail failed, try default
                if (!thumbnailLoaded && level.thumbnailPath != "assets/textures/level_default.png") {
                    SDL_Surface* defaultSurface = IMG_Load("assets/textures/level_default.png");
                    if (defaultSurface) {
                        thumbnail = SDL_CreateTextureFromSurface(renderer, defaultSurface);
                        SDL_FreeSurface(defaultSurface);
                        if (thumbnail) {
                            thumbnailLoaded = true;
                        }
                    }
                }
            } else {
                // Locked levels always use default thumbnail and are semi-transparent
                SDL_Surface* defaultSurface = IMG_Load("assets/textures/level_default.png");
                if (defaultSurface) {
                    thumbnail = SDL_CreateTextureFromSurface(renderer, defaultSurface);
                    SDL_FreeSurface(defaultSurface);
                    if (thumbnail) {
                        thumbnailLoaded = true;
                        thumbnailAlpha = 128;  // 50% opacity for locked levels
                    }
                }
            }
            
            if (thumbnailLoaded && thumbnail) {
                SDL_Rect thumbRect;
                thumbRect.w = THUMBNAIL_WIDTH;
                thumbRect.h = THUMBNAIL_HEIGHT;
                thumbRect.x = thumbX - thumbRect.w / 2;
                thumbRect.y = thumbY - thumbRect.h / 2;
                
                // Set alpha modulation for transparency
                SDL_SetTextureAlphaMod(thumbnail, thumbnailAlpha);
                SDL_RenderCopy(renderer, thumbnail, nullptr, &thumbRect);
                SDL_DestroyTexture(thumbnail);
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
            
            // Draw level title
            if (levelFont) {
                SDL_Color textColor = isUnlocked ? SDL_Color{255, 255, 255, 255} : SDL_Color{150, 150, 150, 255};
                SDL_Surface* textSurface = TTF_RenderUTF8_Blended(levelFont, level.title.c_str(), textColor);
                if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        SDL_Rect textRect;
                        textRect.w = textSurface->w;
                        textRect.h = textSurface->h;
                        textRect.x = panelCenterX - textRect.w / 2;
                        textRect.y = levelAreaY + THUMBNAIL_HEIGHT + 40;
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
            }
            
            // Draw lock icon if locked
            if (!isUnlocked && levelFont) {
                SDL_Color lockColor = {200, 200, 200, 255};
                SDL_Surface* lockSurface = TTF_RenderUTF8_Blended(levelFont, "LOCKED", lockColor);
                if (lockSurface) {
                    SDL_Texture* lockTexture = SDL_CreateTextureFromSurface(renderer, lockSurface);
                    if (lockTexture) {
                        SDL_Rect lockRect;
                        lockRect.w = lockSurface->w;
                        lockRect.h = lockSurface->h;
                        lockRect.x = thumbX - lockRect.w / 2;
                        lockRect.y = thumbY - lockRect.h / 2;
                        SDL_RenderCopy(renderer, lockTexture, nullptr, &lockRect);
                        SDL_DestroyTexture(lockTexture);
                    }
                    SDL_FreeSurface(lockSurface);
                }
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
            SDL_Color continueColor = continueSelected ? SDL_Color{80, 200, 80, 255} : SDL_Color{50, 150, 50, 255};
            SDL_SetRenderDrawColor(renderer, continueColor.r, continueColor.g, continueColor.b, continueColor.a);
            SDL_RenderFillRect(renderer, &continueRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &continueRect);
            
            if (buttonFont) {
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface* textSurface = TTF_RenderUTF8_Blended(buttonFont, "Continue", white);
                if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        SDL_Rect textRect;
                        textRect.w = textSurface->w;
                        textRect.h = textSurface->h;
                        textRect.x = buttonStartX + (buttonWidth - textRect.w) / 2;
                        textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
            }
            
            // Restart button
            int restartX = screenWidth / 2 + buttonSpacing / 2;
            bool restartSelected = (inButtonMode && selectedButtonIndex == 1) || (mouseMode && hoveredButtonIndex == 1);
            SDL_Rect restartRect = {restartX, buttonAreaY, buttonWidth, buttonHeight};
            SDL_Color restartColor = restartSelected ? SDL_Color{200, 150, 100, 255} : SDL_Color{150, 100, 50, 255};
            SDL_SetRenderDrawColor(renderer, restartColor.r, restartColor.g, restartColor.b, restartColor.a);
            SDL_RenderFillRect(renderer, &restartRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &restartRect);
            
            if (buttonFont) {
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface* textSurface = TTF_RenderUTF8_Blended(buttonFont, "Restart", white);
                if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        SDL_Rect textRect;
                        textRect.w = textSurface->w;
                        textRect.h = textSurface->h;
                        textRect.x = restartX + (buttonWidth - textRect.w) / 2;
                        textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
                }
            }
        } else {
            // Draw Play button (disabled if level is locked)
            int playX = screenWidth / 2 - buttonWidth / 2;
            bool playSelected = (inButtonMode && selectedButtonIndex == 0) || (mouseMode && hoveredButtonIndex == 0 && isUnlocked);
            bool playDisabled = !isUnlocked;
            SDL_Rect playRect = {playX, buttonAreaY, buttonWidth, buttonHeight};
            SDL_Color playColor;
            if (playDisabled) {
                playColor = SDL_Color{60, 60, 60, 255};  // Dark gray for disabled
            } else {
                playColor = playSelected ? SDL_Color{80, 200, 80, 255} : SDL_Color{50, 150, 50, 255};
            }
            SDL_SetRenderDrawColor(renderer, playColor.r, playColor.g, playColor.b, playColor.a);
            SDL_RenderFillRect(renderer, &playRect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &playRect);
            
            if (buttonFont) {
                SDL_Color textColor = playDisabled ? SDL_Color{150, 150, 150, 255} : SDL_Color{255, 255, 255, 255};
                SDL_Surface* textSurface = TTF_RenderUTF8_Blended(buttonFont, "Play", textColor);
                if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    if (textTexture) {
                        SDL_Rect textRect;
                        textRect.w = textSurface->w;
                        textRect.h = textSurface->h;
                        textRect.x = playX + (buttonWidth - textRect.w) / 2;
                        textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                        SDL_DestroyTexture(textTexture);
                    }
                    SDL_FreeSurface(textSurface);
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
        SDL_Color backColor = backSelected ? SDL_Color{150, 150, 150, 255} : SDL_Color{100, 100, 100, 255};
        SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
        SDL_RenderFillRect(renderer, &backRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &backRect);
        
        if (buttonFont) {
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* textSurface = TTF_RenderUTF8_Blended(buttonFont, "Back", white);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                if (textTexture) {
                    SDL_Rect textRect;
                    textRect.w = textSurface->w;
                    textRect.h = textSurface->h;
                    textRect.x = backX + (buttonWidth - textRect.w) / 2;
                    textRect.y = buttonAreaY + (buttonHeight - textRect.h) / 2;
                    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                    SDL_DestroyTexture(textTexture);
                }
                SDL_FreeSurface(textSurface);
            }
        }
    }
    
    // Clean up fonts
    if (titleFont) TTF_CloseFont(titleFont);
    if (levelFont) TTF_CloseFont(levelFont);
    if (buttonFont) TTF_CloseFont(buttonFont);
    
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

bool LevelSelectMenu::hasSaveDataForSelectedLevel() const {
    if (selectedLevelIndex < 0 || selectedLevelIndex >= static_cast<int>(levels.size())) {
        return false;
    }
    
    const LevelInfo& level = levels[selectedLevelIndex];
    SaveManager& saveMgr = SaveManager::getInstance();
    
    // Check if save file exists
    if (!saveMgr.saveExists()) {
        return false;
    }
    
    // Check if current level matches selected level
    std::string currentLevel = saveMgr.getCurrentLevel();
    if (currentLevel != level.id || currentLevel.empty()) {
        return false;
    }
    
    // Check if save file has valid level data (background and objects populated)
    return saveMgr.hasValidLevelData("save.json");
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

