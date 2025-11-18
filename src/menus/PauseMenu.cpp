#include "PauseMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../SaveManager.h"
#include <iostream>

PauseMenu::PauseMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Paused");
    setupMenuItems();
}

void PauseMenu::setupMenuItems() {
    clearItems();
    
    addItem("Resume", [this]() { onResume(); });
    addItem("Save Game", [this]() { onSave(); });
    addItem("Settings", [this]() { onSettings(); });
    addItem("Quit to Menu", [this]() { onQuitToMenu(); });
    addItem("Quit to Desktop", [this]() { onQuitToDesktop(); });
}

void PauseMenu::handleCancel() {
    // Cancel closes pause menu (same as Resume)
    onResume();
}

void PauseMenu::onResume() {
    if (menuManager) {
        menuManager->closeMenu();
    }
}

void PauseMenu::onSave() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Save the game
    if (engine->saveGame("save.json")) {
        std::cout << "PauseMenu: Game saved successfully" << std::endl;
        // Optionally show a message or close the menu
        // For now, we'll just save and keep the menu open
    } else {
        std::cerr << "PauseMenu: Failed to save game" << std::endl;
    }
}

void PauseMenu::onSettings() {
    // Settings does nothing for now
}

void PauseMenu::onQuitToMenu() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Stop hosting/client mode
    if (engine->isHosting()) {
        engine->stopHosting();
    }
    if (engine->isClient()) {
        engine->disconnectClient();
    }
    
    // Clear objects (unload level)
    engine->getObjects().clear();
    engine->getQueuedObjects().clear();
    
    // Return to main menu
    menuManager->returnToMainMenu();
}

void PauseMenu::onQuitToDesktop() {
    // Quit the game by setting running to false
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    menuManager->getEngine()->quit();
}

