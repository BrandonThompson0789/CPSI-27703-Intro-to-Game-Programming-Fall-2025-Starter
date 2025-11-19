#include "HostMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../HostManager.h"
#include <iostream>

HostMenu::HostMenu(MenuManager* manager)
    : Menu(manager), state(State::CONNECTING), connectionTimeout(0.0f), 
      roomCode(""), hostingAttemptStarted(false), levelSelectOpened(false) {
    setTitle("Hosting Game");
    setupMenuItems();
}

void HostMenu::setupMenuItems() {
    clearItems();
    updateMenuItems();
}

void HostMenu::updateMenuItems() {
    clearItems();
    
    if (state == State::CONNECTING) {
        addItem("Connecting to server...", [this]() {});
    } else if (state == State::SUCCESS && !roomCode.empty()) {
        addItem("Room Code: " + roomCode, [this]() {});
        addItem("Opening level select...", [this]() {});
    } else if (state == State::FAILED) {
        addItem("Failed to connect to server manager", [this]() {});
        addItem("Returning to menu...", [this]() {});
    } else {
        addItem("Connecting to server...", [this]() {});
    }
}

void HostMenu::onOpen() {
    Menu::onOpen();
    
    // Reset state when menu opens
    state = State::CONNECTING;
    roomCode = "";
    connectionTimeout = 0.0f;
    hostingAttemptStarted = false;
    levelSelectOpened = false;
    
    // Start hosting attempt (this will block for up to 5 seconds, but menu is now visible)
    startHostingAttempt();
}

void HostMenu::startHostingAttempt() {
    if (hostingAttemptStarted) {
        return;
    }
    
    hostingAttemptStarted = true;
    
    if (!menuManager || !menuManager->getEngine()) {
        state = State::FAILED;
        connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Try to start hosting (this blocks for up to 5 seconds waiting for server manager)
    std::string code = engine->startHosting();
    if (!code.empty()) {
        // Success - got room code
        state = State::SUCCESS;
        roomCode = code;
        updateMenuItems();
    } else {
        // Failed to connect
        state = State::FAILED;
        engine->displayMessage("Failed to connect to server manager");
        connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
        updateMenuItems();
    }
}

void HostMenu::update(float deltaTime) {
    Menu::update(deltaTime);
    
    if (state == State::FAILED && connectionTimeout > 0.0f) {
        connectionTimeout -= deltaTime;
        if (connectionTimeout <= 0.0f) {
            // Return to multiplayer menu
            if (menuManager) {
                menuManager->closeMenu();
            }
        }
    } else if (state == State::SUCCESS && !roomCode.empty() && !levelSelectOpened) {
        // Hosting succeeded - open level select menu
        // Only do this once
        levelSelectOpened = true;
        if (menuManager) {
            menuManager->openMenu("level_select");
        }
    } else if (state == State::CONNECTING && hostingAttemptStarted) {
        // Double-check hosting status in case it succeeded but we missed it
        if (menuManager && menuManager->getEngine()) {
            Engine* engine = menuManager->getEngine();
            if (engine->isHosting()) {
                HostManager* hostMgr = engine->getHostManager();
                if (hostMgr) {
                    std::string code = hostMgr->GetRoomCode();
                    if (!code.empty()) {
                        state = State::SUCCESS;
                        roomCode = code;
                        updateMenuItems();
                    }
                }
            } else if (hostingAttemptStarted) {
                // Hosting attempt completed but failed
                // This should have been caught in startHostingAttempt, but check anyway
                if (state == State::CONNECTING) {
                    state = State::FAILED;
                    connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
                }
            }
        }
    }
}

void HostMenu::handleCancel() {
    onBack();
}

void HostMenu::onBack() {
    // Stop hosting if it was started
    if (menuManager && menuManager->getEngine()) {
        Engine* engine = menuManager->getEngine();
        if (engine->isHosting()) {
            engine->stopHosting();
        }
    }
    
    if (menuManager) {
        menuManager->closeMenu();
    }
}

