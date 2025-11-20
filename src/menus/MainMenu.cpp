#include "MainMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include "../SaveManager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

MainMenu::MainMenu(MenuManager* manager)
    : Menu(manager) {
    setTitle("Main Menu");
    setupMenuItems();
}

void MainMenu::setupMenuItems() {
    clearItems();
    
    // Continue - enable if:
    // 1. Save file has valid level data (background and objects populated), OR
    // 2. Progression > 0 AND there's a level with order higher than progression
    SaveManager& saveMgr = SaveManager::getInstance();
    bool hasValidSaveData = saveMgr.hasValidLevelData("save.json");
    int progression = saveMgr.getLevelProgression();
    bool hasNextLevel = saveMgr.hasNextLevelAvailable();
    bool canContinue = hasValidSaveData || (progression > 0 && hasNextLevel);
    addItem("Continue", canContinue, canContinue, [this]() { onContinue(); });
    
    // Play
    addItem("Play", [this]() { onPlay(); });
    
    // Multiplayer
    addItem("Multiplayer", [this]() { onMultiplayer(); });
    
    // Settings
    addItem("Settings", [this]() { onSettings(); });
    
    // Quit
    addItem("Quit", [this]() { onQuit(); });
}

void MainMenu::onOpen() {
    Menu::onOpen();
    
    // Only load the main menu level if no menus exist (coming from game state)
    // If menus already exist, we're navigating within the menu system, so don't reload
    if (menuManager && menuManager->getEngine()) {
        // Check if menu stack is empty (we're coming from game state, not from another menu)
        if (!menuManager->isMenuActive()) {
            Engine* engine = menuManager->getEngine();
            engine->loadFile("assets/levels/level_mainmenu.json");
        }
    }
    
    // Refresh menu items when menu is opened (to update Continue button state)
    setupMenuItems();
}

void MainMenu::onContinue() {
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    SaveManager& saveMgr = SaveManager::getInstance();
    
    // Check if we have valid save data
    bool hasValidSaveData = saveMgr.hasValidLevelData("save.json");
    
    if (hasValidSaveData) {
        // Try to load the save game
        bool loaded = engine->loadGame("save.json");
        
        if (loaded) {
            // Check if level data was actually loaded (has objects)
            if (engine->getObjects().empty()) {
                // This shouldn't happen if hasValidLevelData returned true, but handle it anyway
                std::cerr << "Continue: Save file has no level data despite validation" << std::endl;
                return;
            } else {
                std::cout << "Continue: Loaded save game" << std::endl;
                // Close all menus to start the game
                menuManager->closeAllMenus();
            }
        } else {
            std::cerr << "Continue: Failed to load save game" << std::endl;
        }
    } else {
        // No valid save data, but progression > 0 and next level available
        // Load next available level based on progression
        int progression = saveMgr.getLevelProgression();
        int nextLevelOrder = progression + 10;  // Next level after current progression
        
        // Scan for level with matching order
        std::string levelsDir = "assets/levels";
        std::string levelToLoad;
        
        try {
            if (std::filesystem::exists(levelsDir) && std::filesystem::is_directory(levelsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(levelsDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        std::string filePath = entry.path().string();
                        std::ifstream file(filePath);
                        if (file.is_open()) {
                            nlohmann::json levelJson;
                            try {
                                file >> levelJson;
                                file.close();
                                
                                if (levelJson.contains("order") && levelJson["order"].is_number_integer()) {
                                    int order = levelJson["order"].get<int>();
                                    if (order == nextLevelOrder) {
                                        levelToLoad = filePath;
                                        break;
                                    }
                                }
                            } catch (...) {
                                file.close();
                            }
                        }
                    }
                }
            }
        } catch (...) {
            // Filesystem error, ignore
        }
        
        if (!levelToLoad.empty()) {
            engine->loadFile(levelToLoad);
            std::string levelId = std::filesystem::path(levelToLoad).stem().string();
            saveMgr.setCurrentLevel(levelId);
            std::cout << "Continue: Loaded next available level: " << levelId << std::endl;
            // Close all menus to start the game
            menuManager->closeAllMenus();
        } else {
            std::cerr << "Continue: Could not find next available level" << std::endl;
            // Don't close menu if we can't load a level
        }
    }
}

void MainMenu::onPlay() {
    if (menuManager) {
        menuManager->openMenu("level_select");
    }
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

