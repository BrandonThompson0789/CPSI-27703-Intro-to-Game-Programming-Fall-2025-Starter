#include "Object.h"
#include "Engine.h"
#include "components/Component.h"
#include "components/BodyComponent.h"
#include "components/ComponentLibrary.h"
#include <iostream>

Engine* Object::engineInstance = nullptr;
std::unordered_set<Object*> Object::liveObjects;

Object::Object() {
    liveObjects.insert(this);
}

Object::~Object() {
    if (!markedForDeath) {
        for (auto& component : components) {
            if (component) {
                component->onParentDeath();
            }
        }
    }
    componentMap.clear();
    components.clear();
    liveObjects.erase(this);
}

void Object::setEngine(Engine* engine) {
    engineInstance = engine;
}

Engine* Object::getEngine() {
    return engineInstance;
}

bool Object::isAlive(const Object* object) {
    if (object == nullptr) {
        return false;
    }
    return liveObjects.find(const_cast<Object*>(object)) != liveObjects.end();
}

void Object::update(float deltaTime) {
    if (markedForDeath) {
        return;
    }

    // Update all components
    for (auto& component : components) {
        component->update(deltaTime);
    }
}

void Object::render(SDL_Renderer* renderer) {
    if (markedForDeath) {
        return;
    }

    // Draw all components
    for (auto& component : components) {
        component->draw();
    }
}

nlohmann::json Object::toJson() const {
    nlohmann::json j;
    
    // Serialize name if it exists
    if (!name.empty()) {
        j["name"] = name;
    }
    
    nlohmann::json componentsArray = nlohmann::json::array();
    
    // Serialize all components
    for (const auto& component : components) {
        componentsArray.push_back(component->toJson());
    }
    
    j["components"] = componentsArray;
    return j;
}

void Object::fromJson(const nlohmann::json& data) {
    // Load name if it exists
    if (data.contains("name")) {
        name = data["name"].get<std::string>();
    }
    
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

void Object::markForDeath() {
    if (markedForDeath) {
        return;
    }

    for (auto& component : components) {
        if (component) {
            component->onParentDeath();
        }
    }
    markedForDeath = true;
}

bool Object::isMarkedForDeath() const {
    return markedForDeath;
}