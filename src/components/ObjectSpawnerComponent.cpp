#include "ObjectSpawnerComponent.h"
#include "ComponentLibrary.h"
#include "../Object.h"
#include "../Engine.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>

ObjectSpawnerComponent::ObjectSpawnerComponent(Object& parent)
    : Component(parent)
    , spawnInOrder(true)
    , useNextPosition(true)
    , currentSpawnableIndex(0)
    , currentLocationIndex(0)
    , rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

ObjectSpawnerComponent::ObjectSpawnerComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , spawnInOrder(data.value("spawnInOrder", true))
    , useNextPosition(data.value("useNextPosition", true))
    , currentSpawnableIndex(0)
    , currentLocationIndex(0)
    , rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
{
    // Load spawnable objects
    if (data.contains("spawnableObjects") && data["spawnableObjects"].is_array()) {
        for (const auto& objData : data["spawnableObjects"]) {
            SpawnableObject spawnable;
            
            // Check for template at top level
            if (objData.contains("template")) {
                spawnable.templateName = objData["template"].get<std::string>();
            }
            
            // Store the full object data (can include template or be standalone)
            if (objData.contains("objectData")) {
                spawnable.objectData = objData["objectData"];
                // If objectData has a template but we don't have one at top level, use it
                if (spawnable.templateName.empty() && spawnable.objectData.contains("template")) {
                    spawnable.templateName = spawnable.objectData["template"].get<std::string>();
                }
            } else {
                // If no objectData, use the whole object definition (excluding top-level fields)
                spawnable.objectData = objData;
                // Remove top-level fields that are not part of object definition
                spawnable.objectData.erase("maxSpawns");
                if (spawnable.templateName.empty() && spawnable.objectData.contains("template")) {
                    spawnable.templateName = spawnable.objectData["template"].get<std::string>();
                }
            }
            
            spawnable.maxSpawns = objData.value("maxSpawns", -1);
            spawnable.remainingSpawns = spawnable.maxSpawns;
            
            spawnableObjects.push_back(spawnable);
        }
    }
    
    // Load spawn locations
    if (data.contains("spawnLocations") && data["spawnLocations"].is_array()) {
        for (const auto& locData : data["spawnLocations"]) {
            float x = locData.value("x", 0.0f);
            float y = locData.value("y", 0.0f);
            spawnLocations.push_back(SpawnLocation(x, y));
        }
    }
}

void ObjectSpawnerComponent::update(float deltaTime) {
    // Nothing to update
    (void)deltaTime;
}

void ObjectSpawnerComponent::use(Object& instigator) {
    (void)instigator;  // Instigator not used for now
    
    if (spawnableObjects.empty() || spawnLocations.empty()) {
        return;
    }
    
    spawnObject();
}

void ObjectSpawnerComponent::spawnObject() {
    // Select which object to spawn
    int spawnableIndex = selectSpawnableObjectIndex();
    if (spawnableIndex < 0 || spawnableIndex >= static_cast<int>(spawnableObjects.size())) {
        return;
    }
    
    SpawnableObject& spawnable = spawnableObjects[spawnableIndex];
    
    // Check if we can spawn this object
    if (spawnable.remainingSpawns == 0) {
        return;  // No more spawns available
    }
    
    // Select spawn location
    int locationIndex = selectSpawnLocationIndex();
    if (locationIndex < 0 || locationIndex >= static_cast<int>(spawnLocations.size())) {
        return;
    }
    
    const SpawnLocation& location = spawnLocations[locationIndex];
    
    // Create and queue the object
    createAndQueueObject(spawnable, location);
    
    // Decrement remaining spawns (if not infinite)
    if (spawnable.remainingSpawns > 0) {
        spawnable.remainingSpawns--;
    }
    
    // Update indices for sequential spawning
    if (spawnInOrder) {
        currentSpawnableIndex = (currentSpawnableIndex + 1) % static_cast<int>(spawnableObjects.size());
    }
    
    if (useNextPosition) {
        currentLocationIndex = (currentLocationIndex + 1) % static_cast<int>(spawnLocations.size());
    }
}

int ObjectSpawnerComponent::selectSpawnableObjectIndex() {
    if (spawnableObjects.empty()) {
        return -1;
    }
    
    if (spawnInOrder) {
        // Find next available spawnable object
        int startIndex = currentSpawnableIndex;
        for (int i = 0; i < static_cast<int>(spawnableObjects.size()); ++i) {
            int index = (startIndex + i) % static_cast<int>(spawnableObjects.size());
            const SpawnableObject& spawnable = spawnableObjects[index];
            if (spawnable.remainingSpawns != 0) {
                return index;
            }
        }
        return -1;  // No spawnable objects available
    } else {
        // Random selection from available objects
        std::vector<int> availableIndices;
        for (size_t i = 0; i < spawnableObjects.size(); ++i) {
            if (spawnableObjects[i].remainingSpawns != 0) {
                availableIndices.push_back(static_cast<int>(i));
            }
        }
        
        if (availableIndices.empty()) {
            return -1;
        }
        
        std::uniform_int_distribution<int> dist(0, static_cast<int>(availableIndices.size()) - 1);
        return availableIndices[dist(rng)];
    }
}

int ObjectSpawnerComponent::selectSpawnLocationIndex() {
    if (spawnLocations.empty()) {
        return -1;
    }
    
    if (useNextPosition) {
        return currentLocationIndex;
    } else {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(spawnLocations.size()) - 1);
        return dist(rng);
    }
}

void ObjectSpawnerComponent::createAndQueueObject(const SpawnableObject& spawnable, const SpawnLocation& location) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        std::cerr << "ObjectSpawnerComponent: Engine not available!" << std::endl;
        return;
    }
    
    // Build the object definition
    nlohmann::json objectDef;
    
    // If we have a template name, use Engine's buildObjectDefinition to resolve it
    if (!spawnable.templateName.empty()) {
        nlohmann::json templateData;
        templateData["template"] = spawnable.templateName;
        // Merge with any overrides from objectData
        if (!spawnable.objectData.empty() && spawnable.objectData.is_object()) {
            // Merge objectData as overrides (but remove template field if present)
            for (auto& [key, value] : spawnable.objectData.items()) {
                if (key != "template") {  // Don't override the template field
                    templateData[key] = value;
                }
            }
        }
        objectDef = engine->buildObjectDefinition(templateData);
    } else {
        // No template, use objectData directly
        objectDef = spawnable.objectData;
    }
    
    // Ensure objectDef has components array
    if (!objectDef.contains("components") || !objectDef["components"].is_array()) {
        if (!objectDef.is_object()) {
            objectDef = nlohmann::json::object();
        }
        objectDef["components"] = nlohmann::json::array();
    }
    
    // Find or create BodyComponent to set position
    bool foundBodyComponent = false;
    for (auto& component : objectDef["components"]) {
        if (component.contains("type") && component["type"] == "BodyComponent") {
            component["posX"] = location.x;
            component["posY"] = location.y;
            foundBodyComponent = true;
            break;
        }
    }
    
    // If no BodyComponent found, add one
    if (!foundBodyComponent) {
        nlohmann::json bodyComponent;
        bodyComponent["type"] = "BodyComponent";
        bodyComponent["posX"] = location.x;
        bodyComponent["posY"] = location.y;
        objectDef["components"].push_back(bodyComponent);
    }
    
    // Create the object
    auto newObject = std::make_unique<Object>();
    newObject->fromJson(objectDef);
    
    // Queue it to be added to the engine
    engine->queueObject(std::move(newObject));
}

nlohmann::json ObjectSpawnerComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["spawnInOrder"] = spawnInOrder;
    j["useNextPosition"] = useNextPosition;
    
    // Serialize spawnable objects
    nlohmann::json spawnableArray = nlohmann::json::array();
    for (const auto& spawnable : spawnableObjects) {
        nlohmann::json objData;
        if (!spawnable.templateName.empty()) {
            objData["template"] = spawnable.templateName;
        }
        if (!spawnable.objectData.empty()) {
            objData["objectData"] = spawnable.objectData;
        }
        objData["maxSpawns"] = spawnable.maxSpawns;
        objData["remainingSpawns"] = spawnable.remainingSpawns;
        spawnableArray.push_back(objData);
    }
    j["spawnableObjects"] = spawnableArray;
    
    // Serialize spawn locations
    nlohmann::json locationsArray = nlohmann::json::array();
    for (const auto& location : spawnLocations) {
        nlohmann::json locData;
        locData["x"] = location.x;
        locData["y"] = location.y;
        locationsArray.push_back(locData);
    }
    j["spawnLocations"] = locationsArray;
    
    return j;
}

int ObjectSpawnerComponent::getTotalRemainingSpawns() const {
    int total = 0;
    for (const auto& spawnable : spawnableObjects) {
        if (spawnable.remainingSpawns < 0) {
            return -1;  // Infinite spawns
        }
        total += spawnable.remainingSpawns;
    }
    return total;
}

static ComponentRegistrar<ObjectSpawnerComponent> registrar("ObjectSpawnerComponent");

