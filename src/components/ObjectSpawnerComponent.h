#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <random>

struct SpawnableObject {
    std::string templateName;  // Template name from objectData.json (can be empty)
    nlohmann::json objectData;  // Full object definition (can use template or be standalone)
    int maxSpawns;              // -1 for infinite
    int remainingSpawns;         // Current remaining spawns
    
    SpawnableObject() : maxSpawns(-1), remainingSpawns(-1) {}
};

struct SpawnLocation {
    float x;
    float y;
    
    SpawnLocation(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
};

/**
 * Component that spawns objects when 'use' is triggered.
 * Supports templates, spawn limits, sequential/random spawning, and position selection.
 */
class ObjectSpawnerComponent : public Component {
public:
    ObjectSpawnerComponent(Object& parent);
    ObjectSpawnerComponent(Object& parent, const nlohmann::json& data);
    ~ObjectSpawnerComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    void use(Object& instigator) override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "ObjectSpawnerComponent"; }

    // Getters for debug drawing
    const std::vector<SpawnLocation>& getSpawnLocations() const { return spawnLocations; }
    const std::vector<SpawnableObject>& getSpawnableObjects() const { return spawnableObjects; }
    int getTotalRemainingSpawns() const;

private:
    void spawnObject();
    int selectSpawnableObjectIndex();
    int selectSpawnLocationIndex();
    void createAndQueueObject(const SpawnableObject& spawnable, const SpawnLocation& location);

    std::vector<SpawnableObject> spawnableObjects;
    std::vector<SpawnLocation> spawnLocations;
    
    // Spawning options
    bool spawnInOrder;          // true = sequential, false = random
    bool useNextPosition;        // true = next position, false = random position
    
    // State tracking
    int currentSpawnableIndex;   // For sequential spawning
    int currentLocationIndex;    // For sequential position selection
    
    // Random number generation
    mutable std::mt19937 rng;
};

