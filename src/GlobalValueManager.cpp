#include "GlobalValueManager.h"
#include <iostream>

GlobalValueManager& GlobalValueManager::getInstance() {
    static GlobalValueManager instance;
    return instance;
}

float GlobalValueManager::getValue(const std::string& name) const {
    auto it = values.find(name);
    if (it != values.end()) {
        return it->second;
    }
    return 0.0f;
}

void GlobalValueManager::setValue(const std::string& name, float value) {
    values[name] = value;
}

void GlobalValueManager::modifyValue(const std::string& name, float delta) {
    values[name] = getValue(name) + delta;
}

bool GlobalValueManager::hasValue(const std::string& name) const {
    return values.find(name) != values.end();
}

void GlobalValueManager::removeValue(const std::string& name) {
    values.erase(name);
}

void GlobalValueManager::clear() {
    values.clear();
}

nlohmann::json GlobalValueManager::toJson() const {
    nlohmann::json data = nlohmann::json::object();
    for (const auto& [name, value] : values) {
        data[name] = value;
    }
    return data;
}

void GlobalValueManager::fromJson(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }
    
    for (const auto& [key, value] : data.items()) {
        if (value.is_number()) {
            values[key] = value.get<float>();
        }
    }
}

