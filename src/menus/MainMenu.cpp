#include "MainMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include <iostream>

MainMenu::MainMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Main Menu");
    setupMenuItems();
}

void MainMenu::setupMenuItems() {
    clearItems();
    
    // Continue (disabled for now)
    addItem("Continue", false, [this]() { onContinue(); });
    items.back().enabled = false;
    
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
    // Continue is disabled for now
}

void MainMenu::onPlay() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    engine->loadFile("assets/level1.json");
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

