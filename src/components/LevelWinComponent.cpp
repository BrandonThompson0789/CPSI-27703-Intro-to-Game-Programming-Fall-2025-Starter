#include "LevelWinComponent.h"
#include "ComponentLibrary.h"
#include "../Engine.h"
#include "../SaveManager.h"
#include "../Object.h"
#include "../menus/MenuManager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <climits>
#include <nlohmann/json.hpp>

LevelWinComponent::LevelWinComponent(Object& parent)
    : Component(parent)
    , targetLevelName("")
    , updateProgression(true)
    , progressionIncrement(1) {
}

LevelWinComponent::LevelWinComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , targetLevelName(data.value("targetLevelName", std::string("")))
    , updateProgression(data.value("updateProgression", true))
    , progressionIncrement(data.value("progressionIncrement", 1)) {
}

void LevelWinComponent::update(float deltaTime) {
    // No per-frame update needed
    (void)deltaTime;
}

void LevelWinComponent::draw() {
    // No drawing needed
}

void LevelWinComponent::use(Object& instigator) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        std::cerr << "[LevelWinComponent] Engine not available" << std::endl;
        return;
    }

    SaveManager& saveMgr = SaveManager::getInstance();
    MenuManager* menuMgr = engine->getMenuManager();
    
    // Get current level's order value and progression BEFORE any updates
    // This is critical - we must check progression before modifying it
    int currentLevelOrder = engine->getCurrentLevelOrder();
    int currentProgression = saveMgr.getLevelProgression();
    
    std::cout << "[LevelWinComponent] Checking win condition - Progression: " << currentProgression 
              << ", Level Order: " << currentLevelOrder << std::endl;
    
    // Check if progression is already equal to or higher than current level order
    // If so, go to main menu (don't update progression or load next level)
    // This means the player has already completed this level before
    if (currentProgression >= currentLevelOrder) {
        // Load main menu and open level select, highlighting current level
        std::cout << "[LevelWinComponent] Progression (" << currentProgression 
                  << ") is already >= level order (" << currentLevelOrder 
                  << "), returning to main menu" << std::endl;
        
        // Get current level ID for highlighting
        std::string currentLevelId = engine->getCurrentLoadedLevel();
        
        // Save the game without level data (only once)
        saveMgr.setCurrentLevel("level_mainmenu");
        if (!saveMgr.saveGameWithoutLevelData(engine, "save.json")) {
            std::cerr << "[LevelWinComponent] Warning: Failed to save game" << std::endl;
        }
        
        // Load main menu
        engine->queueLevelLoad("assets/levels/level_mainmenu.json");
        
        // Open level select menu and highlight current level
        if (menuMgr) {
            menuMgr->openMenuWithLevelSelect(currentLevelId);
        }
        return;
    }
    
    // Progression is less than level order - update progression and auto-load next level
    if (updateProgression) {
        saveMgr.setLevelProgression(currentLevelOrder);
        std::cout << "[LevelWinComponent] Updated progression from " << currentProgression 
                  << " to " << currentLevelOrder << std::endl;
    }

    // Determine which level to load
    std::string levelToLoad = targetLevelName;
    
    if (levelToLoad.empty()) {
        // Find next available level based on updated progression
        int progression = saveMgr.getLevelProgression();
        std::string levelsDir = "assets/levels";
        std::string nextLevelPath = "";
        int lowestOrder = INT_MAX;
        
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
                                    
                                    // Find level with lowest order above current progression
                                    if (order > progression && order < lowestOrder) {
                                        lowestOrder = order;
                                        nextLevelPath = filePath;
                                    }
                                }
                            } catch (...) {
                                file.close();
                            }
                        }
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[LevelWinComponent] Filesystem error: " << e.what() << std::endl;
        }
        
        if (!nextLevelPath.empty()) {
            levelToLoad = nextLevelPath;
        } else {
            std::cerr << "[LevelWinComponent] No next level found. Progression: " << progression << std::endl;
            // Still save even if no next level found
            saveMgr.setCurrentLevel(engine->getCurrentLoadedLevel());
            saveMgr.saveGame(engine, "save.json");
            return;
        }
    } else {
        // If targetLevelName is provided but not a full path, try to construct it
        if (levelToLoad.find("assets/levels/") == std::string::npos) {
            if (levelToLoad.find(".json") == std::string::npos) {
                levelToLoad = "assets/levels/" + levelToLoad + ".json";
            } else {
                levelToLoad = "assets/levels/" + levelToLoad;
            }
        }
    }

    // Update save data with new level and save without level data (only once)
    saveMgr.setCurrentLevel(levelToLoad);
    if (!saveMgr.saveGameWithoutLevelData(engine, "save.json")) {
        std::cerr << "[LevelWinComponent] Warning: Failed to save game before level transition" << std::endl;
    }
    
    // Queue the level to be loaded at the start of the next frame
    // This prevents the crash from destroying objects during update
    std::cout << "[LevelWinComponent] Queuing level load: " << levelToLoad << std::endl;
    engine->queueLevelLoad(levelToLoad);
}

nlohmann::json LevelWinComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    if (!targetLevelName.empty()) {
        data["targetLevelName"] = targetLevelName;
    }
    data["updateProgression"] = updateProgression;
    data["progressionIncrement"] = progressionIncrement;
    return data;
}

// Register this component type with the library
static ComponentRegistrar<LevelWinComponent> registrar("LevelWinComponent");

