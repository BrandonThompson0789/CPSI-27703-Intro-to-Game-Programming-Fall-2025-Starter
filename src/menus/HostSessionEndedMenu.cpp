#include "HostSessionEndedMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../ClientManager.h"
#include <iostream>

HostSessionEndedMenu::HostSessionEndedMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Host Session Ended");
    setupMenuItems();
}

void HostSessionEndedMenu::setupMenuItems() {
    clearItems();
    addItem("The host has ended the session.", [this]() {});
    addItem("Return to Main Menu", [this]() { onReturnToMainMenu(); });
}

void HostSessionEndedMenu::handleCancel() {
    onReturnToMainMenu();
}

void HostSessionEndedMenu::onReturnToMainMenu() {
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

