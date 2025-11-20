#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

class GlobalValueManager {
public:
    static GlobalValueManager& getInstance();
    
    // Get a global value (returns 0.0 if not found)
    float getValue(const std::string& name) const;
    
    // Set a global value (creates if doesn't exist)
    void setValue(const std::string& name, float value);
    
    // Modify a global value (adds to existing, creates if doesn't exist)
    void modifyValue(const std::string& name, float delta);
    
    // Check if a value exists
    bool hasValue(const std::string& name) const;
    
    // Remove a value
    void removeValue(const std::string& name);
    
    // Clear all values
    void clear();
    
    // Serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& data);
    
private:
    GlobalValueManager() = default;
    ~GlobalValueManager() = default;
    GlobalValueManager(const GlobalValueManager&) = delete;
    GlobalValueManager& operator=(const GlobalValueManager&) = delete;
    
    std::unordered_map<std::string, float> values;
};

