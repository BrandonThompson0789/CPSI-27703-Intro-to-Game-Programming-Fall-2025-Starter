#include "SaveManager.h"
#include "Engine.h"
#include "BackgroundManager.h"
#include "Object.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

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

bool SaveManager::loadSaveData(const std::string& saveFilePath) {
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

