#include "MainMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../SaveManager.h"
#include <iostream>

MainMenu::MainMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Main Menu");
    setupMenuItems();
}

void MainMenu::setupMenuItems() {
    clearItems();
    
    // Continue - enable if save exists
    bool hasSave = SaveManager::getInstance().saveExists();
    addItem("Continue", hasSave, [this]() { onContinue(); });
    
    // Play
    addItem("Play", [this]() { onPlay(); });
    
    // Multiplayer
    addItem("Multiplayer", [this]() { onMultiplayer(); });
    
    // Settings
    addItem("Settings", [this]() { onSettings(); });
    
    // Quit
    addItem("Quit", [this]() { onQuit(); });
}

void MainMenu::onContinue() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    // Load the save game
    if (engine->loadGame("save.json")) {
        std::cout << "Continue: Loaded save game" << std::endl;
        // Current level is already set from SaveManager when loading save data
        menuManager->closeMenu();
    } else {
        std::cerr << "Continue: Failed to load save game" << std::endl;
    }
}

void MainMenu::onPlay() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    engine->loadFile("assets/level1.json");
    
    // Update SaveManager with current level
    SaveManager::getInstance().setCurrentLevel("level1");
    
    menuManager->closeMenu();
}

void MainMenu::onMultiplayer() {
    if (menuManager) {
        menuManager->openMenu("multiplayer");
    }
}

void MainMenu::onSettings() {
    // Settings does nothing for now
}

void MainMenu::onQuit() {
    if (menuManager) {
        menuManager->openMenu("quit_confirm");
    }
}

void MainMenu::handleCancel() {
    // On Main Menu, cancel opens Quit confirmation
    onQuit();
}

