#include "JoinMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../ClientManager.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <iostream>

JoinMenu::JoinMenu(MenuManager* manager)
    : Menu(manager), roomCodeInput(""), joinButtonEnabled(false),
      selectedButtonIndex(0), hoveredButtonIndex(-1), mouseMode(false),
      font(nullptr), buttonFont(nullptr), roomCodeTexture(nullptr),
      joinButtonTexture(nullptr), pasteButtonTexture(nullptr),
      backButtonTexture(nullptr), placeholderTexture(nullptr),
      roomCodeTextureWidth(0), roomCodeTextureHeight(0),
      state(State::INPUT), connectionTimeout(0.0f) {
    setTitle("Join Game");
    setupMenuItems();
    loadResources();
    updateRoomCodeTexture();
}

JoinMenu::~JoinMenu() {
    // Disable text input
    SDL_StopTextInput();
    unloadResources();
}

void JoinMenu::setupMenuItems() {
    clearItems();
    // Items are handled via custom rendering
}

void JoinMenu::onOpen() {
    Menu::onOpen();
    roomCodeInput = "";
    joinButtonEnabled = false;
    selectedButtonIndex = 0;
    hoveredButtonIndex = -1;
    mouseMode = false;
    state = State::INPUT;
    connectionTimeout = 0.0f;
    updateRoomCodeTexture();
    updateJoinButtonState();
    
    // Enable text input
    SDL_StartTextInput();
}

void JoinMenu::update(float deltaTime) {
    Menu::update(deltaTime);
    
    if (state == State::FAILED && connectionTimeout > 0.0f) {
        connectionTimeout -= deltaTime;
        if (connectionTimeout <= 0.0f) {
            // Return to multiplayer menu
            if (menuManager) {
                menuManager->closeMenu();
            }
        }
    } else if (state == State::CONNECTING) {
        // Check if connection succeeded
        if (menuManager && menuManager->getEngine()) {
            Engine* engine = menuManager->getEngine();
            if (engine->isClient()) {
                // Connection successful - close menu and start game
                state = State::INPUT;  // Reset state
                menuManager->closeAllMenus();
            }
        }
    }
}

void JoinMenu::handleNavigation(float upDown, float leftRight) {
    mouseMode = false;
    hoveredButtonIndex = -1;
    
    // Horizontal navigation between buttons
    if (leftRight > 0.5f) {
        selectedButtonIndex = (selectedButtonIndex + 1) % 2;
    } else if (leftRight < -0.5f) {
        selectedButtonIndex = (selectedButtonIndex - 1 + 2) % 2;
    }
}

void JoinMenu::handleConfirm() {
    // Allow back button to work even when connecting
    if (selectedButtonIndex == 1) {
        onBack();
        return;
    }
    
    // Only allow join when in INPUT state
    if (state != State::INPUT) {
        return;
    }
    
    if (selectedButtonIndex == 0 && joinButtonEnabled) {
        onJoin();
    }
}

void JoinMenu::handleCancel() {
    // Allow cancel even when connecting - disconnect and go back
    onBack();
}

void JoinMenu::handleMouse(int mouseX, int mouseY, bool mousePressed) {
    mouseMode = true;
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    // Button positions (matching render)
    int buttonY = screenHeight / 2 + 100;
    int buttonWidth = 150;
    int buttonHeight = 40;
    int buttonSpacing = 20;
    
    int joinX = screenWidth / 2 - buttonWidth - buttonSpacing / 2;
    int backX = screenWidth / 2 + buttonSpacing / 2;
    
    // Paste button
    int pasteX = screenWidth / 2 + 150;
    int pasteY = screenHeight / 2 - 20;
    int pasteWidth = 100;
    int pasteHeight = 30;
    
    hoveredButtonIndex = -1;
    
    // Check paste button (only in INPUT state)
    if (state == State::INPUT && mouseX >= pasteX && mouseX < pasteX + pasteWidth &&
        mouseY >= pasteY && mouseY < pasteY + pasteHeight) {
        if (mousePressed) {
            onPaste();
        }
    }
    // Check join button (only in INPUT state)
    else if (state == State::INPUT && mouseX >= joinX && mouseX < joinX + buttonWidth &&
             mouseY >= buttonY && mouseY < buttonY + buttonHeight) {
        hoveredButtonIndex = 0;
        if (mousePressed && joinButtonEnabled) {
            onJoin();
        }
    }
    // Check back button (always works, even when connecting)
    else if (mouseX >= backX && mouseX < backX + buttonWidth &&
             mouseY >= buttonY && mouseY < buttonY + buttonHeight) {
        hoveredButtonIndex = 1;
        if (mousePressed) {
            onBack();
        }
    }
}

void JoinMenu::handleTextInput(const std::string& text) {
    if (state != State::INPUT) {
        return;
    }
    
    // Only allow alphanumeric characters and limit to 6 characters
    for (char c : text) {
        if (roomCodeInput.length() < ROOM_CODE_LENGTH) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                roomCodeInput += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }
    }
    updateRoomCodeTexture();
    updateJoinButtonState();
}

void JoinMenu::handleBackspace() {
    if (state != State::INPUT && roomCodeInput.empty()) {
        return;
    }
    
    if (!roomCodeInput.empty()) {
        roomCodeInput.pop_back();
        updateRoomCodeTexture();
        updateJoinButtonState();
    }
}

void JoinMenu::onJoin() {
    if (!joinButtonEnabled || state != State::INPUT) {
        return;
    }
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Try to connect
    state = State::CONNECTING;
    bool success = engine->connectAsClient(roomCodeInput);
    
    if (!success) {
        state = State::FAILED;
        engine->displayMessage("Failed to connect to server manager");
        connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
    } else {
        // Connection initiated - wait for confirmation in update()
        // The menu will close when connection is confirmed
    }
}

void JoinMenu::onPaste() {
    if (state != State::INPUT) {
        return;
    }
    
    if (SDL_HasClipboardText()) {
        char* clipboardText = SDL_GetClipboardText();
        if (clipboardText) {
            std::string text(clipboardText);
            SDL_free(clipboardText);
            
            // Extract first 6 alphanumeric characters
            roomCodeInput = "";
            for (char c : text) {
                if (roomCodeInput.length() < ROOM_CODE_LENGTH) {
                    if (std::isalnum(static_cast<unsigned char>(c))) {
                        roomCodeInput += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    }
                }
            }
            updateRoomCodeTexture();
            updateJoinButtonState();
        }
    }
}

void JoinMenu::onBack() {
    // Disconnect client if connected or connecting
    if (menuManager && menuManager->getEngine()) {
        Engine* engine = menuManager->getEngine();
        if (engine->isClient()) {
            engine->disconnectClient();
        }
    }
    
    // Reset state
    state = State::INPUT;
    
    if (menuManager) {
        menuManager->closeMenu();
    }
}

void JoinMenu::updateJoinButtonState() {
    joinButtonEnabled = (roomCodeInput.length() == ROOM_CODE_LENGTH);
}

void JoinMenu::loadResources() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer) {
        return;
    }
    
    if (TTF_WasInit()) {
        font = TTF_OpenFont("assets/fonts/ARIAL.TTF", 24);
        buttonFont = TTF_OpenFont("assets/fonts/ARIAL.TTF", 20);
    }
    
    // Pre-render button textures
    if (buttonFont) {
        SDL_Color white = {255, 255, 255, 255};
        
        SDL_Surface* joinSurface = TTF_RenderUTF8_Blended(buttonFont, "Join", white);
        if (joinSurface) {
            joinButtonTexture = SDL_CreateTextureFromSurface(renderer, joinSurface);
            SDL_FreeSurface(joinSurface);
        }
        
        SDL_Surface* pasteSurface = TTF_RenderUTF8_Blended(buttonFont, "Paste", white);
        if (pasteSurface) {
            pasteButtonTexture = SDL_CreateTextureFromSurface(renderer, pasteSurface);
            SDL_FreeSurface(pasteSurface);
        }
        
        SDL_Surface* backSurface = TTF_RenderUTF8_Blended(buttonFont, "Back", white);
        if (backSurface) {
            backButtonTexture = SDL_CreateTextureFromSurface(renderer, backSurface);
            SDL_FreeSurface(backSurface);
        }
    }
    
    // Create placeholder texture
    if (font) {
        SDL_Color gray = {150, 150, 150, 255};
        SDL_Surface* placeholderSurface = TTF_RenderUTF8_Blended(font, "Enter room code...", gray);
        if (placeholderSurface) {
            placeholderTexture = SDL_CreateTextureFromSurface(renderer, placeholderSurface);
            SDL_FreeSurface(placeholderSurface);
        }
    }
    
    updateRoomCodeTexture();
}

void JoinMenu::unloadResources() {
    if (roomCodeTexture) {
        SDL_DestroyTexture(roomCodeTexture);
        roomCodeTexture = nullptr;
    }
    if (joinButtonTexture) {
        SDL_DestroyTexture(joinButtonTexture);
        joinButtonTexture = nullptr;
    }
    if (pasteButtonTexture) {
        SDL_DestroyTexture(pasteButtonTexture);
        pasteButtonTexture = nullptr;
    }
    if (backButtonTexture) {
        SDL_DestroyTexture(backButtonTexture);
        backButtonTexture = nullptr;
    }
    if (placeholderTexture) {
        SDL_DestroyTexture(placeholderTexture);
        placeholderTexture = nullptr;
    }
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    if (buttonFont) {
        TTF_CloseFont(buttonFont);
        buttonFont = nullptr;
    }
}

void JoinMenu::updateRoomCodeTexture() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer || !font) {
        return;
    }
    
    if (roomCodeTexture) {
        SDL_DestroyTexture(roomCodeTexture);
        roomCodeTexture = nullptr;
    }
    
    SDL_Color white = {255, 255, 255, 255};
    std::string displayText = roomCodeInput.empty() ? "" : roomCodeInput;
    
    if (!displayText.empty()) {
        SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, displayText.c_str(), white);
        if (textSurface) {
            roomCodeTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            roomCodeTextureWidth = textSurface->w;
            roomCodeTextureHeight = textSurface->h;
            SDL_FreeSurface(textSurface);
        }
    }
}

bool JoinMenu::render() {
    if (!menuManager || !menuManager->getEngine()) {
        return true;
    }
    
    SDL_Renderer* renderer = menuManager->getEngine()->getRenderer();
    if (!renderer) {
        return true;
    }
    
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    // Draw semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 230);
    SDL_Rect bgRect = {0, 0, screenWidth, screenHeight};
    SDL_RenderFillRect(renderer, &bgRect);
    
    // Draw title
    if (TTF_WasInit() && font) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* titleSurface = TTF_RenderUTF8_Blended(font, getTitle().c_str(), white);
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
    
    // Draw room code input area
    int inputY = screenHeight / 2 - 20;
    int inputWidth = 200;
    int inputHeight = 40;
    int inputX = screenWidth / 2 - inputWidth / 2;
    
    // Draw input box background
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect inputRect = {inputX, inputY, inputWidth, inputHeight};
    SDL_RenderFillRect(renderer, &inputRect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &inputRect);
    
    // Draw room code text or placeholder
    if (roomCodeTexture) {
        SDL_Rect textRect;
        textRect.w = roomCodeTextureWidth;
        textRect.h = roomCodeTextureHeight;
        textRect.x = inputX + (inputWidth - textRect.w) / 2;
        textRect.y = inputY + (inputHeight - textRect.h) / 2;
        SDL_RenderCopy(renderer, roomCodeTexture, nullptr, &textRect);
    } else if (placeholderTexture && roomCodeInput.empty()) {
        SDL_Rect textRect;
        SDL_QueryTexture(placeholderTexture, nullptr, nullptr, &textRect.w, &textRect.h);
        textRect.x = inputX + (inputWidth - textRect.w) / 2;
        textRect.y = inputY + (inputHeight - textRect.h) / 2;
        SDL_RenderCopy(renderer, placeholderTexture, nullptr, &textRect);
    }
    
    // Draw paste button
    int pasteX = screenWidth / 2 + 150;
    int pasteY = inputY;
    int pasteWidth = 100;
    int pasteHeight = 30;
    
    SDL_Color pasteColor = {100, 150, 200, 255};
    SDL_SetRenderDrawColor(renderer, pasteColor.r, pasteColor.g, pasteColor.b, pasteColor.a);
    SDL_Rect pasteRect = {pasteX, pasteY, pasteWidth, pasteHeight};
    SDL_RenderFillRect(renderer, &pasteRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &pasteRect);
    
    if (pasteButtonTexture) {
        SDL_Rect textRect;
        SDL_QueryTexture(pasteButtonTexture, nullptr, nullptr, &textRect.w, &textRect.h);
        textRect.x = pasteX + (pasteWidth - textRect.w) / 2;
        textRect.y = pasteY + (pasteHeight - textRect.h) / 2;
        SDL_RenderCopy(renderer, pasteButtonTexture, nullptr, &textRect);
    }
    
    // Draw buttons
    int buttonY = screenHeight / 2 + 100;
    int buttonWidth = 150;
    int buttonHeight = 40;
    int buttonSpacing = 20;
    
    int joinX = screenWidth / 2 - buttonWidth - buttonSpacing / 2;
    int backX = screenWidth / 2 + buttonSpacing / 2;
    
    // Join button
    bool joinSelected = (selectedButtonIndex == 0 && !mouseMode) || (hoveredButtonIndex == 0 && mouseMode);
    SDL_Color joinColor = joinButtonEnabled 
        ? (joinSelected ? SDL_Color{80, 200, 80, 255} : SDL_Color{50, 150, 50, 255})
        : SDL_Color{60, 60, 60, 255};
    SDL_SetRenderDrawColor(renderer, joinColor.r, joinColor.g, joinColor.b, joinColor.a);
    SDL_Rect joinRect = {joinX, buttonY, buttonWidth, buttonHeight};
    SDL_RenderFillRect(renderer, &joinRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &joinRect);
    
    if (joinButtonTexture) {
        uint8_t alpha = joinButtonEnabled ? 255 : 150;
        SDL_SetTextureAlphaMod(joinButtonTexture, alpha);
        SDL_Rect textRect;
        SDL_QueryTexture(joinButtonTexture, nullptr, nullptr, &textRect.w, &textRect.h);
        textRect.x = joinX + (buttonWidth - textRect.w) / 2;
        textRect.y = buttonY + (buttonHeight - textRect.h) / 2;
        SDL_RenderCopy(renderer, joinButtonTexture, nullptr, &textRect);
    }
    
    // Back button
    bool backSelected = (selectedButtonIndex == 1 && !mouseMode) || (hoveredButtonIndex == 1 && mouseMode);
    SDL_Color backColor = backSelected ? SDL_Color{150, 150, 150, 255} : SDL_Color{100, 100, 100, 255};
    SDL_SetRenderDrawColor(renderer, backColor.r, backColor.g, backColor.b, backColor.a);
    SDL_Rect backRect = {backX, buttonY, buttonWidth, buttonHeight};
    SDL_RenderFillRect(renderer, &backRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &backRect);
    
    if (backButtonTexture) {
        SDL_Rect textRect;
        SDL_QueryTexture(backButtonTexture, nullptr, nullptr, &textRect.w, &textRect.h);
        textRect.x = backX + (buttonWidth - textRect.w) / 2;
        textRect.y = buttonY + (buttonHeight - textRect.h) / 2;
        SDL_RenderCopy(renderer, backButtonTexture, nullptr, &textRect);
    }
    
    // Draw status message if connecting or failed
    if (state == State::CONNECTING) {
        if (font) {
            SDL_Color yellow = {255, 255, 0, 255};
            SDL_Surface* statusSurface = TTF_RenderUTF8_Blended(font, "Connecting...", yellow);
            if (statusSurface) {
                SDL_Texture* statusTexture = SDL_CreateTextureFromSurface(renderer, statusSurface);
                if (statusTexture) {
                    SDL_Rect statusRect;
                    statusRect.w = statusSurface->w;
                    statusRect.h = statusSurface->h;
                    statusRect.x = (screenWidth - statusRect.w) / 2;
                    statusRect.y = buttonY + buttonHeight + 20;
                    SDL_RenderCopy(renderer, statusTexture, nullptr, &statusRect);
                    SDL_DestroyTexture(statusTexture);
                }
                SDL_FreeSurface(statusSurface);
            }
        }
    }
    
    return true;
}

