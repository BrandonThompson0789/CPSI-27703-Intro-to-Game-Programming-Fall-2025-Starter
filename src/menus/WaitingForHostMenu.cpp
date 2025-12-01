#include "WaitingForHostMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../ClientManager.h"
#include <iostream>

WaitingForHostMenu::WaitingForHostMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Waiting for Host");
    setupMenuItems();
}

void WaitingForHostMenu::setupMenuItems() {
    clearItems();
    addItem("Host is setting up game...", [this]() {});
    addItem("Return to Main Menu", [this]() { onBack(); });
}

void WaitingForHostMenu::update(float deltaTime) {
    Menu::update(deltaTime);
    
    // Check if host has loaded a level (client received init package)
    checkHostStatus();
}

void WaitingForHostMenu::checkHostStatus() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // If client is no longer connected, go back to main menu
    if (!engine->isClient()) {
        if (menuManager) {
            menuManager->returnToMainMenu();
        }
        return;
    }
    
    // If client has received init package, host has loaded a level - close this menu
    auto clientMgr = engine->getClientManager();
    if (clientMgr && clientMgr->HasReceivedInitPackage()) {
        // Host has loaded a level, close the waiting menu
        if (menuManager) {
            menuManager->closeMenu();
        }
    }
}

void WaitingForHostMenu::handleCancel() {
    onBack();
}

void WaitingForHostMenu::onBack() {
    // Disconnect client and return to main menu
    if (menuManager && menuManager->getEngine()) {
        Engine* engine = menuManager->getEngine();
        if (engine->isClient()) {
            engine->disconnectClient();
        }
    }
    
    if (menuManager) {
        menuManager->returnToMainMenu();
    }
}

