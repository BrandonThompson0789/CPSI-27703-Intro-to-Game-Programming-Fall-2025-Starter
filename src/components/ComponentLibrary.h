#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <typeindex>

class Object;

// Factory function type for creating components
using ComponentFactory = std::function<std::unique_ptr<Component>(Object&, const nlohmann::json&)>;

/**
 * ComponentLibrary - Registry for component types to enable serialization
 * Each component type registers itself with a unique name and factory function
 */
class ComponentLibrary {
public:
    // Get the singleton instance
    static ComponentLibrary& getInstance();
    
    // Register a component type with its factory function and type_index
    void registerComponent(const std::string& typeName, ComponentFactory factory, std::type_index typeIndex);
    
    // Create a component from JSON data
    std::unique_ptr<Component> createComponent(const std::string& typeName, Object& parent, const nlohmann::json& data);
    
    // Check if a component type is registered
    bool isRegistered(const std::string& typeName) const;
    
    // Get all registered component type names
    std::vector<std::string> getRegisteredTypes() const;
    
    // Get type_index for a component type name
    std::type_index getTypeIndex(const std::string& typeName) const;
    
private:
    ComponentLibrary() = default;
    std::unordered_map<std::string, ComponentFactory> factories;
    std::unordered_map<std::string, std::type_index> typeIndices;
};

/**
 * Helper class for automatic component registration
 * Usage: In each component's .cpp file, create a static instance:
 * static ComponentRegistrar<MyComponent> registrar("MyComponent");
 */
template<typename T>
class ComponentRegistrar {
public:
    ComponentRegistrar(const std::string& typeName) {
        ComponentLibrary::getInstance().registerComponent(typeName, 
            [](Object& parent, const nlohmann::json& data) -> std::unique_ptr<Component> {
                return std::make_unique<T>(parent, data);
            },
            std::type_index(typeid(T)));
    }
};

