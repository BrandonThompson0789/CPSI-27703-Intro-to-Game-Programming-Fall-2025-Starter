#include "ComponentLibrary.h"
#include <stdexcept>

ComponentLibrary& ComponentLibrary::getInstance() {
    static ComponentLibrary instance;
    return instance;
}

void ComponentLibrary::registerComponent(const std::string& typeName, ComponentFactory factory, std::type_index typeIndex) {
    factories[typeName] = factory;
    typeIndices.insert_or_assign(typeName, typeIndex);
}

std::unique_ptr<Component> ComponentLibrary::createComponent(const std::string& typeName, Object& parent, const nlohmann::json& data) {
    auto it = factories.find(typeName);
    if (it == factories.end()) {
        throw std::runtime_error("Component type not registered: " + typeName);
    }
    return it->second(parent, data);
}

bool ComponentLibrary::isRegistered(const std::string& typeName) const {
    return factories.find(typeName) != factories.end();
}

std::vector<std::string> ComponentLibrary::getRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(factories.size());
    for (const auto& pair : factories) {
        types.push_back(pair.first);
    }
    return types;
}

std::type_index ComponentLibrary::getTypeIndex(const std::string& typeName) const {
    auto it = typeIndices.find(typeName);
    if (it == typeIndices.end()) {
        throw std::runtime_error("Component type not registered: " + typeName);
    }
    return it->second;
}

