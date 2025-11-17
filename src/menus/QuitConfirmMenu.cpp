#include "QuitConfirmMenu.h"
#include "MenuManager.h"
#include "../Engine.h"

QuitConfirmMenu::QuitConfirmMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Are You Sure?");
    setupMenuItems();
}

void QuitConfirmMenu::setupMenuItems() {
    clearItems();
    
    addItem("Yes", [this]() { onYes(); });
    addItem("No", [this]() { onNo(); });
}

void QuitConfirmMenu::handleCancel() {
    // Cancel goes back to main menu
    onNo();
}

void QuitConfirmMenu::onYes() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    // Quit the game by setting running to false
    menuManager->getEngine()->quit();
}

void QuitConfirmMenu::onNo() {
    if (menuManager) {
        menuManager->returnToMainMenu();
    }
}

