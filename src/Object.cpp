#include "Object.h"
#include "components/Component.h"
#include "components/BodyComponent.h"
#include "components/ComponentLibrary.h"
#include <iostream>

Object::~Object() = default;

void Object::update() {
    // Update all components
    for (auto& component : components) {
        component->update();
    }
}

void Object::render(SDL_Renderer* renderer) {
    // Draw all components
    for (auto& component : components) {
        component->draw();
    }
}

nlohmann::json Object::toJson() const {
    nlohmann::json j;
    nlohmann::json componentsArray = nlohmann::json::array();
    
    // Serialize all components
    for (const auto& component : components) {
        componentsArray.push_back(component->toJson());
    }
    
    j["components"] = componentsArray;
    return j;
}

void Object::fromJson(const nlohmann::json& data) {
    // Clear existing components
    components.clear();
    componentMap.clear();
    
    if (!data.contains("components")) {
        return;
    }
    
    ComponentLibrary& library = ComponentLibrary::getInstance();
    
    // Create components from JSON
    for (const auto& componentData : data["components"]) {
        if (!componentData.contains("type")) {
            continue;
        }
        
        std::string typeName = componentData["type"].get<std::string>();
        
        try {
            // Use the component library to create the component
            auto component = library.createComponent(typeName, *this, componentData);
            
            // Add to our component lists
            Component* ptr = component.get();
            components.push_back(std::move(component));
            
            // Get the type_index from the library and populate componentMap
            std::type_index typeIdx = library.getTypeIndex(typeName);
            componentMap[typeIdx] = ptr;
        } catch (const std::exception& e) {
            // Component type not registered or other error
            // Skip this component
            std::cerr << "Warning: Failed to create component '" << typeName << "': " << e.what() << std::endl;
        }
    }
}