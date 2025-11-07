#include "PhysicsMaterial.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

struct MaterialRegistry {
    std::unordered_map<std::string, int> nameToId;
    std::vector<std::string> idToName;
    std::mutex mutex;
};

MaterialRegistry& registry() {
    static MaterialRegistry instance;
    return instance;
}

void initializeIfNeeded(MaterialRegistry& reg) {
    if (!reg.idToName.empty()) {
        return;
    }

    reg.idToName.push_back("default");
    reg.nameToId.emplace("default", 0);
}

} // namespace

void PhysicsMaterialLibrary::ensureInitialized() {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    initializeIfNeeded(reg);
}

int PhysicsMaterialLibrary::getMaterialId(const std::string& name) {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    initializeIfNeeded(reg);

    std::string key = name.empty() ? "default" : toLower(name);
    auto it = reg.nameToId.find(key);
    if (it != reg.nameToId.end()) {
        return it->second;
    }

    int id = static_cast<int>(reg.idToName.size());
    reg.nameToId.emplace(key, id);
    reg.idToName.push_back(key);
    return id;
}

const std::string& PhysicsMaterialLibrary::getMaterialName(int materialId) {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    initializeIfNeeded(reg);

    if (materialId >= 0 && materialId < static_cast<int>(reg.idToName.size())) {
        return reg.idToName[materialId];
    }

    static const std::string unknown = "unknown";
    return unknown;
}

bool PhysicsMaterialLibrary::isValid(int materialId) {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mutex);
    initializeIfNeeded(reg);
    return materialId >= 0 && materialId < static_cast<int>(reg.idToName.size());
}

