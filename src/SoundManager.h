#pragma once

#include <SDL_mixer.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <mutex>
#include <optional>
#include <utility>

struct CollisionImpact;
class Engine;

/**
 * SoundManager owns SDL_mixer initialization and provides easy access to
 * sound effect collections defined in a JSON file. Each collection contains
 * multiple variations of the same effect (e.g., several impact sounds) and
 * SoundManager will randomly rotate through the available clips whenever the
 * collection is played to minimize repetition.
 *
 * The manager also knows how to resolve collision impacts into sound
 * collections based on the colliding materials and the impact intensity.
 */
class SoundManager {
public:
    static SoundManager& getInstance();

    bool init(const std::string& soundConfigPath, Engine* engineContext);
    void shutdown();

    bool isInitialized() const { return initialized; }

    void playCollection(const std::string& collectionName, float baseVolume = 1.0f, int channel = -1);
    void playCollectionAt(const std::string& collectionName, float worldX, float worldY, float baseVolume = 1.0f, int channel = -1);
    void playImpactSound(const CollisionImpact& impact);

    const std::string& getSoundConfigPath() const { return configPath; }

private:
    struct SoundEntry {
        std::string path;
        Mix_Chunk* chunk = nullptr;
        int volume = -1;
    };

    struct SoundCollection {
        std::vector<SoundEntry> entries;
        int lastIndex = -1;
    };

    struct ImpactCollections {
        std::string soft;
        std::string medium;
        std::string hard;
    };

    SoundManager();
    ~SoundManager();

    void freeAllChunks();
    bool loadConfig(const std::string& soundConfigPath, nlohmann::json& outData);
    void loadCollections(const nlohmann::json& data);
    void loadImpactRules(const nlohmann::json& data);
    const ImpactCollections* resolveImpactRule(const std::string& materialA,
                                               const std::string& materialB) const;
    std::string classifyIntensity(float approachSpeed) const;
    Mix_Chunk* pickChunkForCollection(SoundCollection& collection);
    void playCollectionInternal(const std::string& collectionName,
                                float baseVolume,
                                std::optional<std::pair<float, float>> worldPosition,
                                int channel);
    float computeSpatialAttenuation(float worldX, float worldY) const;

    bool initialized = false;
    std::string configPath;

    int frequency = 44100;
    Uint16 format = AUDIO_S16LSB;
    int channels = 2;
    int chunkSize = 2048;
    int allocatedChannels = 32;

    float softThreshold = 1.5f;
    float mediumThreshold = 4.0f;
    float hardThreshold = 8.0f;

    std::unordered_map<std::string, SoundCollection> collections;
    std::unordered_map<std::string, ImpactCollections> impactRules;
    ImpactCollections defaultImpactCollections;

    std::mt19937 rng;
    std::mutex mutex;
    Engine* engine = nullptr;
    bool spatialAttenuationEnabled = true;
    float maxOffscreenDistance = 600.0f;
};


