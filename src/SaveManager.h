#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>

class Engine;

class SaveManager {
public:
    static SaveManager& getInstance();
    
    // Save current game state
    bool saveGame(Engine* engine, const std::string& saveFilePath = "save.json");
    
    // Save game without level data (for level win - clears background and objects)
    bool saveGameWithoutLevelData(Engine* engine, const std::string& saveFilePath = "save.json");
    
    // Load overall save data (metadata, progress, settings)
    bool loadSaveData(const std::string& saveFilePath = "save.json");
    
    // Check if a save file exists
    bool saveExists(const std::string& saveFilePath = "save.json") const;
    
    // Get the current level name from save data
    std::string getCurrentLevel() const { return currentLevel; }
    
    // Get level progress
    const nlohmann::json& getLevelProgress() const { return levelProgress; }
    
    // Get settings
    const nlohmann::json& getSettings() const { return settings; }
    
    // Set level progress
    void setLevelProgress(const nlohmann::json& progress) { levelProgress = progress; }
    
    // Set settings
    void setSettings(const nlohmann::json& settingsData) { settings = settingsData; }
    
    // Set current level
    void setCurrentLevel(const std::string& level) { currentLevel = level; }
    
    // Get level progression (integer value)
    int getLevelProgression() const;
    
    // Set level progression (integer value)
    void setLevelProgression(int progression);
    
    // Clear save data
    void clear();
    
    // Ensure save file exists (create default if it doesn't)
    bool ensureSaveFileExists(const std::string& saveFilePath = "save.json");
    
    // Check if save file has valid level data (background and objects populated)
    bool hasValidLevelData(const std::string& saveFilePath = "save.json") const;
    
    // Check if there's a level with order higher than current progression
    bool hasNextLevelAvailable() const;
    
private:
    SaveManager() = default;
    ~SaveManager() = default;
    SaveManager(const SaveManager&) = delete;
    SaveManager& operator=(const SaveManager&) = delete;
    
    std::string currentLevel;
    nlohmann::json levelProgress;
    nlohmann::json settings;
};

#endif // SAVE_MANAGER_H

