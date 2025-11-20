#include "SaveManager.h"
#include "Engine.h"
#include "BackgroundManager.h"
#include "Object.h"
#include "GlobalValueManager.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>

SaveManager& SaveManager::getInstance() {
    static SaveManager instance;
    return instance;
}

bool SaveManager::saveGame(Engine* engine, const std::string& saveFilePath) {
    if (!engine) {
        std::cerr << "SaveManager: Cannot save - Engine is null" << std::endl;
        return false;
    }
    
    nlohmann::json saveData;
    
    // Create metadata
    nlohmann::json metadata;
    metadata["version"] = "1.0";
    
    // Get current time as ISO 8601 string
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%S");
    metadata["lastSaved"] = ss.str();
    
    metadata["currentLevel"] = currentLevel.empty() ? "level1" : currentLevel;
    
    // Ensure levelProgress has progression field
    if (!levelProgress.is_object()) {
        levelProgress = nlohmann::json::object();
    }
    if (!levelProgress.contains("progression")) {
        levelProgress["progression"] = 0;
    }
    
    metadata["levelProgress"] = levelProgress;
    metadata["settings"] = settings;
    
    saveData["metadata"] = metadata;
    
    // Save background configuration
    BackgroundManager* bgManager = engine->getBackgroundManager();
    if (bgManager) {
        nlohmann::json bgJson = bgManager->toJson();
        // Convert array format to object format with "layers" key
        nlohmann::json bgObject;
        bgObject["layers"] = bgJson;
        saveData["background"] = bgObject;
    }
    
    // Save all objects
    nlohmann::json objectsArray = nlohmann::json::array();
    auto& objects = engine->getObjects();
    for (const auto& object : objects) {
        if (!object->isMarkedForDeath()) {
            objectsArray.push_back(object->toJson());
        }
    }
    saveData["objects"] = objectsArray;
    
    // Save global values
    GlobalValueManager& gvm = GlobalValueManager::getInstance();
    saveData["globalValues"] = gvm.toJson();
    
    // Write to file
    std::ofstream file(saveFilePath);
    if (!file.is_open()) {
        std::cerr << "SaveManager: Could not open save file for writing: " << saveFilePath << std::endl;
        return false;
    }
    
    try {
        file << saveData.dump(4); // Pretty print with 4-space indent
        file.close();
        std::cout << "SaveManager: Game saved successfully to " << saveFilePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SaveManager: Error writing save file: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

bool SaveManager::saveGameWithoutLevelData(Engine* engine, const std::string& saveFilePath) {
    if (!engine) {
        std::cerr << "SaveManager: Cannot save - Engine is null" << std::endl;
        return false;
    }
    
    nlohmann::json saveData;
    
    // Create metadata
    nlohmann::json metadata;
    metadata["version"] = "1.0";
    
    // Get current time as ISO 8601 string
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%S");
    metadata["lastSaved"] = ss.str();
    
    metadata["currentLevel"] = currentLevel.empty() ? "level1" : currentLevel;
    
    // Ensure levelProgress has progression field
    if (!levelProgress.is_object()) {
        levelProgress = nlohmann::json::object();
    }
    if (!levelProgress.contains("progression")) {
        levelProgress["progression"] = 0;
    }
    
    metadata["levelProgress"] = levelProgress;
    metadata["settings"] = settings;
    
    saveData["metadata"] = metadata;
    
    // Don't save background or objects (level data cleared on win)
    saveData["background"] = nlohmann::json::object();
    saveData["background"]["layers"] = nlohmann::json::array();
    saveData["objects"] = nlohmann::json::array();
    
    // Save global values
    GlobalValueManager& gvm = GlobalValueManager::getInstance();
    saveData["globalValues"] = gvm.toJson();
    
    // Write to file
    std::ofstream file(saveFilePath);
    if (!file.is_open()) {
        std::cerr << "SaveManager: Could not open save file for writing: " << saveFilePath << std::endl;
        return false;
    }
    
    try {
        file << saveData.dump(4); // Pretty print with 4-space indent
        file.close();
        std::cout << "SaveManager: Game saved (without level data) successfully to " << saveFilePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SaveManager: Error writing save file: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

bool SaveManager::loadSaveData(const std::string& saveFilePath) {
    // Ensure save file exists before trying to load
    if (!saveExists(saveFilePath)) {
        if (!ensureSaveFileExists(saveFilePath)) {
            return false;
        }
    }
    
    std::ifstream file(saveFilePath);
    if (!file.is_open()) {
        std::cerr << "SaveManager: Could not open save file: " << saveFilePath << std::endl;
        return false;
    }
    
    nlohmann::json saveData;
    try {
        file >> saveData;
        file.close();
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "SaveManager: JSON parsing error: " << e.what() << std::endl;
        file.close();
        return false;
    }
    
    // Load metadata
    if (saveData.contains("metadata") && saveData["metadata"].is_object()) {
        const auto& metadata = saveData["metadata"];
        
        if (metadata.contains("currentLevel") && metadata["currentLevel"].is_string()) {
            currentLevel = metadata["currentLevel"].get<std::string>();
        }
        
        if (metadata.contains("levelProgress")) {
            levelProgress = metadata["levelProgress"];
        }
        
        if (metadata.contains("settings")) {
            settings = metadata["settings"];
        }
        
        // Load global values from save file
        if (saveData.contains("globalValues") && saveData["globalValues"].is_object()) {
            GlobalValueManager& gvm = GlobalValueManager::getInstance();
            gvm.fromJson(saveData["globalValues"]);
            std::cout << "SaveManager: Loaded global values from save file" << std::endl;
        }
        
        std::cout << "SaveManager: Loaded save data (level: " << currentLevel << ")" << std::endl;
        return true;
    }
    
    std::cerr << "SaveManager: Save file missing metadata" << std::endl;
    return false;
}

bool SaveManager::saveExists(const std::string& saveFilePath) const {
    std::ifstream file(saveFilePath);
    bool exists = file.good();
    file.close();
    return exists;
}

void SaveManager::clear() {
    currentLevel.clear();
    levelProgress = nlohmann::json::object();
    settings = nlohmann::json::object();
}

bool SaveManager::ensureSaveFileExists(const std::string& saveFilePath) {
    if (saveExists(saveFilePath)) {
        return true;  // File already exists
    }
    
    // Create default save file
    nlohmann::json saveData;
    nlohmann::json metadata;
    metadata["version"] = "1.0";
    
    // Get current time
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%S");
    metadata["lastSaved"] = ss.str();
    
    metadata["currentLevel"] = "";
    metadata["levelProgress"] = nlohmann::json::object();
    metadata["levelProgress"]["progression"] = 0;  // Default progression is 0
    metadata["settings"] = nlohmann::json::object();
    
    saveData["metadata"] = metadata;
    saveData["background"] = nlohmann::json::object();
    saveData["background"]["layers"] = nlohmann::json::array();
    saveData["objects"] = nlohmann::json::array();
    
    // Write to file
    std::ofstream file(saveFilePath);
    if (!file.is_open()) {
        std::cerr << "SaveManager: Could not create default save file: " << saveFilePath << std::endl;
        return false;
    }
    
    try {
        file << saveData.dump(4);
        file.close();
        std::cout << "SaveManager: Created default save file: " << saveFilePath << std::endl;
        
        // Load the default data into memory
        currentLevel = "";
        levelProgress = metadata["levelProgress"];
        settings = metadata["settings"];
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SaveManager: Error creating default save file: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

int SaveManager::getLevelProgression() const {
    if (levelProgress.is_object() && levelProgress.contains("progression") && levelProgress["progression"].is_number_integer()) {
        return levelProgress["progression"].get<int>();
    }
    return 0;  // Default progression
}

void SaveManager::setLevelProgression(int progression) {
    if (!levelProgress.is_object()) {
        levelProgress = nlohmann::json::object();
    }
    levelProgress["progression"] = progression;
}

bool SaveManager::hasValidLevelData(const std::string& saveFilePath) const {
    if (!saveExists(saveFilePath)) {
        return false;
    }
    
    std::ifstream file(saveFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    nlohmann::json saveData;
    try {
        file >> saveData;
        file.close();
    } catch (const nlohmann::json::exception& e) {
        file.close();
        return false;
    }
    
    // Check if background exists and has layers (even if empty array, it's valid structure)
    bool hasBackground = saveData.contains("background") && saveData["background"].is_object();
    
    // Check if objects exist and is an array with at least one object
    bool hasObjects = saveData.contains("objects") && 
                      saveData["objects"].is_array() && 
                      saveData["objects"].size() > 0;
    
    // Valid level data means both background and objects are present
    // Objects must have at least one item to be considered valid
    return hasBackground && hasObjects;
}

bool SaveManager::hasNextLevelAvailable() const {
    int progression = getLevelProgression();
    if (progression <= 0) {
        return false;
    }
    
    // Look for a level with order > progression
    std::string levelsDir = "assets/levels";
    
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
                                if (order > progression) {
                                    return true;  // Found a level with higher order
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
        // Filesystem error, return false
        return false;
    }
    
    return false;  // No level found with order > progression
}

