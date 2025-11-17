#include "MultiplayerMenu.h"
#include "MenuManager.h"
#include "../Engine.h"

MultiplayerMenu::MultiplayerMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Multiplayer");
    setupMenuItems();
}

void MultiplayerMenu::setupMenuItems() {
    clearItems();
    
    addItem("Host", [this]() { onHost(); });
    addItem("Join", [this]() { onJoin(); });
    addItem("Go Back", [this]() { onGoBack(); });
}

void MultiplayerMenu::handleCancel() {
    // Cancel goes back to main menu
    onGoBack();
}

void MultiplayerMenu::onHost() {
    // Host does nothing for now
}

void MultiplayerMenu::onJoin() {
    // Join does nothing for now
}

void MultiplayerMenu::onGoBack() {
    if (menuManager) {
        menuManager->returnToMainMenu();
    }
}

