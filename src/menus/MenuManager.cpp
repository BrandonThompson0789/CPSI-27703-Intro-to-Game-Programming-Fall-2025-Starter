#include "MenuManager.h"
#include "LevelSelectMenu.h"
#include "../Engine.h"
#include "../InputManager.h"
#include "Menu.h"
#include "MainMenu.h"
#include "QuitConfirmMenu.h"
#include "MultiplayerMenu.h"
#include "PauseMenu.h"
#include "LevelSelectMenu.h"
#include "HostMenu.h"
#include "JoinMenu.h"
#include "WaitingForHostMenu.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>

MenuManager::MenuManager(Engine* engine)
    : engine(engine), initialized(false), menuJustOpened(false) {
}

MenuManager::~MenuManager() {
    cleanup();
}

bool MenuManager::init() {
    if (initialized) {
        return true;
    }
    
    // Menu system initialization
    // (Could load fonts here if needed)
    
    initialized = true;
    return true;
}

void MenuManager::cleanup() {
    while (!menuStack.empty()) {
        menuStack.pop();
    }
    initialized = false;
}

void MenuManager::update(float deltaTime) {
    if (!initialized) return;
    
    Menu* currentMenu = getCurrentMenu();
    if (!currentMenu) return;
    
    currentMenu->update(deltaTime);
    
    // Re-check menu after update (it might have been closed)
    currentMenu = getCurrentMenu();
    if (!currentMenu) return;
    
    // Handle keyboard/controller navigation
    InputManager& inputMgr = InputManager::getInstance();
    
    // Check for up/down navigation
    float moveUp = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_UP);
    float moveDown = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_DOWN);
    float moveLeft = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_LEFT);
    float moveRight = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_RIGHT);
    
    // Check controllers
    for (int i = 0; i < 4; ++i) {
        if (inputMgr.isInputSourceActive(i)) {
            float up = inputMgr.getInputValue(i, GameAction::MOVE_UP);
            float down = inputMgr.getInputValue(i, GameAction::MOVE_DOWN);
            if (up > moveUp) moveUp = up;
            if (down > moveDown) moveDown = down;
        }
    }
    
    float upDown = moveDown - moveUp;
    float leftRight = moveRight - moveLeft;
    
    // Debounce navigation - only navigate on button press, not while held
    static float lastUpDown = 0.0f;
    static float lastLeftRight = 0.0f;
    bool upDownPressed = (upDown > 0.5f && lastUpDown <= 0.5f) || (upDown < -0.5f && lastUpDown >= -0.5f);
    bool leftRightPressed = (leftRight > 0.5f && lastLeftRight <= 0.5f) || (leftRight < -0.5f && lastLeftRight >= -0.5f);
    
    if (upDownPressed || leftRightPressed) {
        // Re-check menu before navigation (it might have been closed)
        currentMenu = getCurrentMenu();
        if (currentMenu) {
            currentMenu->handleNavigation(upDown, leftRight);
        }
    }
    lastUpDown = upDown;
    lastLeftRight = leftRight;
    
    // Check for confirm/select
    float interact = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_INTERACT);
    for (int i = 0; i < 4; ++i) {
        if (inputMgr.isInputSourceActive(i)) {
            float inter = inputMgr.getInputValue(i, GameAction::ACTION_INTERACT);
            if (inter > interact) interact = inter;
        }
    }
    
    static bool lastInteractState = false;
    bool currentInteractState = (interact > 0.5f);
    if (currentInteractState && !lastInteractState) {
        currentMenu = getCurrentMenu();
        if (currentMenu) {
            currentMenu->handleConfirm();
        }
    }
    lastInteractState = currentInteractState;
    
    // Handle pause action (can close menus/return to previous)
    float pause = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_PAUSE);
    for (int i = 0; i < 4; ++i) {
        if (inputMgr.isInputSourceActive(i)) {
            float p = inputMgr.getInputValue(i, GameAction::ACTION_PAUSE);
            if (p > pause) pause = p;
        }
    }
    
    static bool lastPauseState = false;
    bool currentPauseState = (pause > 0.5f);
    
    // Skip pause input check if menu was just opened this frame (prevents immediate close)
    if (!menuJustOpened) {
        if (currentPauseState && !lastPauseState) {
            currentMenu = getCurrentMenu();
            if (currentMenu) {
                currentMenu->handleCancel();
            }
        }
    }
    
    // Update pause state (always update, even if menu was just opened)
    lastPauseState = currentPauseState;
    
    // Reset flag after processing
    if (menuJustOpened) {
        menuJustOpened = false;
    }
    
    // Handle mouse - check menu is still valid
    currentMenu = getCurrentMenu();
    if (currentMenu) {
        int mouseX, mouseY;
        inputMgr.getMousePosition(mouseX, mouseY);
        bool mousePressed = inputMgr.wasMouseButtonPressedThisFrame(SDL_BUTTON_LEFT);
        currentMenu->handleMouse(mouseX, mouseY, mousePressed);
    }
}

void MenuManager::render() {
    if (!initialized) return;
    
    Menu* currentMenu = getCurrentMenu();
    if (!currentMenu || !engine) {
        return;
    }
    
    // Check if menu has custom rendering
    if (currentMenu->render()) {
        return;  // Menu handled its own rendering
    }
    
    SDL_Renderer* renderer = engine->getRenderer();
    if (!renderer) {
        return;
    }
    
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    const std::vector<MenuItem>& items = currentMenu->getItems();
    const std::string& title = currentMenu->getTitle();
    int selectedIndex = currentMenu->getSelectedIndex();
    int hoveredIndex = currentMenu->getHoveredIndex();
    
    // Draw semi-transparent background box
    SDL_Rect menuRect;
    menuRect.w = 400;
    menuRect.h = static_cast<int>(items.size() * 60) + 100;
    menuRect.x = (screenWidth - menuRect.w) / 2;
    menuRect.y = (screenHeight - menuRect.h) / 2;
    
    // Draw background with alpha
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
    SDL_RenderFillRect(renderer, &menuRect);
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &menuRect);
    
    // Load fonts
    TTF_Font* font = nullptr;
    TTF_Font* titleFont = nullptr;
    
    if (TTF_WasInit()) {
        titleFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 32);
        font = TTF_OpenFont("assets/fonts/ARIAL.TTF", 24);
    }
    
    // Draw title
    if (titleFont && !title.empty()) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* titleSurface = TTF_RenderUTF8_Blended(titleFont, title.c_str(), white);
        if (titleSurface) {
            SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
            if (titleTexture) {
                SDL_Rect titleRect;
                titleRect.w = titleSurface->w;
                titleRect.h = titleSurface->h;
                titleRect.x = menuRect.x + (menuRect.w - titleRect.w) / 2;
                titleRect.y = menuRect.y + 20;
                SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);
                SDL_DestroyTexture(titleTexture);
            }
            SDL_FreeSurface(titleSurface);
        }
    }
    
    // Draw menu items
    if (font) {
        int yOffset = menuRect.y + 80;
        int itemHeight = 50;
        
        for (size_t i = 0; i < items.size(); ++i) {
            const MenuItem& item = items[i];
            SDL_Color color;
            
            // Determine color based on state
            if (!item.enabled) {
                color = {128, 128, 128, 255}; // Gray for disabled
            } else if (hoveredIndex >= 0 && static_cast<size_t>(hoveredIndex) == i) {
                color = {255, 255, 0, 255}; // Yellow for hovered
            } else if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) == i) {
                color = {100, 200, 255, 255}; // Light blue for selected
            } else {
                color = {255, 255, 255, 255}; // White for normal
            }
            
            std::string displayText = item.text;
            /*if (!item.enabled) {
                displayText += " (Disabled)";
            }*/
            
            SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, displayText.c_str(), color);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                if (textTexture) {
                    SDL_Rect textRect;
                    textRect.w = textSurface->w;
                    textRect.h = textSurface->h;
                    textRect.x = menuRect.x + (menuRect.w - textRect.w) / 2;
                    textRect.y = yOffset + static_cast<int>(i) * itemHeight;
                    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                    SDL_DestroyTexture(textTexture);
                }
                SDL_FreeSurface(textSurface);
            }
        }
        
        TTF_CloseFont(font);
    }
    
    if (titleFont) {
        TTF_CloseFont(titleFont);
    }
}

void MenuManager::handleEvent(const SDL_Event& event) {
    if (!initialized) return;
    
    Menu* currentMenu = getCurrentMenu();
    if (!currentMenu) return;
    
    // Handle text input for menus that need it (like JoinMenu)
    switch (event.type) {
        case SDL_TEXTINPUT:
            // Forward text input to current menu
            currentMenu->handleTextInput(event.text.text);
            break;
            
        case SDL_KEYDOWN:
            // Handle backspace for text input
            if (event.key.keysym.sym == SDLK_BACKSPACE && !event.key.repeat) {
                currentMenu->handleBackspace();
            }
            break;
    }
}

void MenuManager::openMenu(const std::string& menuName) {
    auto menu = createMenu(menuName);
    if (menu) {
        menu->onOpen();
        menuStack.push(std::move(menu));
        menuJustOpened = true;  // Mark that a menu was just opened
    }
}

void MenuManager::openMenuWithLevelSelect(const std::string& levelId) {
    // Create level select menu and set pending selection before opening
    auto menu = std::make_unique<LevelSelectMenu>(this);
    if (menu) {
        menu->setPendingLevelSelection(levelId);
        menu->onOpen();
        menuStack.push(std::move(menu));
        menuJustOpened = true;
    }
}

void MenuManager::closeMenu() {
    if (!menuStack.empty()) {
        Menu* currentMenu = menuStack.top().get();
        if (currentMenu) {
            currentMenu->onClose();
        }
        menuStack.pop();
    }
}

void MenuManager::closeAllMenus() {
    // Close all menus
    while (!menuStack.empty()) {
        Menu* currentMenu = menuStack.top().get();
        if (currentMenu) {
            currentMenu->onClose();
        }
        menuStack.pop();
    }
}

void MenuManager::returnToMainMenu() {
    // Close all menus
    while (!menuStack.empty()) {
        Menu* currentMenu = menuStack.top().get();
        if(currentMenu->getTitle() == "Main Menu") {
            return;
        }
        if (currentMenu) {
            currentMenu->onClose();
        }
        menuStack.pop();
    }
    
    // Open main menu
    openMenu("main");
}

bool MenuManager::isMenuActive() const {
    return !menuStack.empty();
}

bool MenuManager::shouldPauseGame() {
    // Pause game when menu is active, except when main menu is in the stack
    if (!isMenuActive()) {
        return false;
    }
    
    // Check if main menu exists anywhere in the menu stack
    // We need to check all menus in the stack, so we'll temporarily move them to a vector
    // and restore them after checking
    std::vector<std::unique_ptr<Menu>> tempMenus;
    
    // Pop all menus into vector
    while (!menuStack.empty()) {
        tempMenus.push_back(std::move(menuStack.top()));
        menuStack.pop();
    }
    
    // Check each menu
    bool hasMainMenu = false;
    for (const auto& menu : tempMenus) {
        if (menu && dynamic_cast<MainMenu*>(menu.get())) {
            hasMainMenu = true;
            break;
        }
    }
    
    // Restore the stack (in reverse order since we popped them)
    for (auto it = tempMenus.rbegin(); it != tempMenus.rend(); ++it) {
        menuStack.push(std::move(*it));
    }
    
    // If main menu is in stack, don't pause
    return !hasMainMenu;
}

std::unique_ptr<Menu> MenuManager::createMenu(const std::string& menuName) {
    if (menuName == "main") {
        return std::make_unique<MainMenu>(this);
    } else if (menuName == "quit_confirm") {
        return std::make_unique<QuitConfirmMenu>(this);
    } else if (menuName == "multiplayer") {
        return std::make_unique<MultiplayerMenu>(this);
    } else if (menuName == "pause") {
        return std::make_unique<PauseMenu>(this);
    } else if (menuName == "level_select") {
        return std::make_unique<LevelSelectMenu>(this);
    } else if (menuName == "host") {
        return std::make_unique<HostMenu>(this);
    } else if (menuName == "join") {
        return std::make_unique<JoinMenu>(this);
    } else if (menuName == "waiting_for_host") {
        return std::make_unique<WaitingForHostMenu>(this);
    }
    
    return nullptr;
}

void MenuManager::selectLevelInLevelSelect(const std::string& levelId) {
    if (menuStack.empty()) {
        return;
    }
    
    Menu* currentMenu = menuStack.top().get();
    if (!currentMenu) {
        return;
    }
    
    // Try to cast to LevelSelectMenu
    // We'll need to check the menu type or use dynamic_cast
    // For now, let's check if it's a level select menu by checking the title
    if (currentMenu->getTitle() == "Level Select") {
        // Cast to LevelSelectMenu and call selectLevelById
        LevelSelectMenu* levelSelectMenu = dynamic_cast<LevelSelectMenu*>(currentMenu);
        if (levelSelectMenu) {
            levelSelectMenu->selectLevelById(levelId);
        }
    }
}

Menu* MenuManager::getCurrentMenu() const {
    if (menuStack.empty()) {
        return nullptr;
    }
    return menuStack.top().get();
}

