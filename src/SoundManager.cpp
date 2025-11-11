#include "SoundManager.h"

#include "CollisionManager.h"
#include "Engine.h"
#include "PhysicsMaterial.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace {

std::string makeMaterialKey(const std::string& a, const std::string& b) {
    if (a <= b) {
        return a + "|" + b;
    }
    return b + "|" + a;
}

} // namespace

SoundManager& SoundManager::getInstance() {
    static SoundManager instance;
    return instance;
}

SoundManager::SoundManager() : rng(std::random_device{}()) {
}

SoundManager::~SoundManager() {
    shutdown();
}

bool SoundManager::init(const std::string& soundConfigPath, Engine* engineContext) {
    std::scoped_lock lock(mutex);

    if (initialized) {
        engine = engineContext;
        return true;
    }

    engine = engineContext;
    configPath = soundConfigPath;
    spatialAttenuationEnabled = true;
    maxOffscreenDistance = 600.0f;

    nlohmann::json configData;
    if (!loadConfig(soundConfigPath, configData)) {
        std::cerr << "SoundManager::init: Failed to load config '" << soundConfigPath << "'" << std::endl;
        return false;
    }

    if (Mix_OpenAudio(frequency, format, channels, chunkSize) < 0) {
        std::cerr << "SoundManager::init: Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        freeAllChunks();
        return false;
    }

    Mix_AllocateChannels(allocatedChannels);

    collections.clear();
    impactRules.clear();

    softThreshold = 1.5f;
    mediumThreshold = 4.0f;
    hardThreshold = 8.0f;
    defaultImpactCollections = {};

    if (configData.contains("collections")) {
        loadCollections(configData["collections"]);
    }
    if (configData.contains("impacts")) {
        loadImpactRules(configData["impacts"]);
    }

    initialized = true;
    return true;
}

void SoundManager::shutdown() {
    std::scoped_lock lock(mutex);

    if (!initialized) {
        return;
    }

    Mix_HaltChannel(-1);
    freeAllChunks();
    Mix_CloseAudio();
    initialized = false;
    engine = nullptr;
}

void SoundManager::freeAllChunks() {
    for (auto& [name, collection] : collections) {
        for (auto& entry : collection.entries) {
            if (entry.chunk) {
                Mix_FreeChunk(entry.chunk);
                entry.chunk = nullptr;
            }
        }
    }
    collections.clear();
    impactRules.clear();
    defaultImpactCollections = {};
}

bool SoundManager::loadConfig(const std::string& soundConfigPath, nlohmann::json& outData) {
    std::ifstream file(soundConfigPath);
    if (!file.is_open()) {
        std::cerr << "SoundManager::loadConfig: Unable to open " << soundConfigPath << std::endl;
        return false;
    }

    try {
        file >> outData;
    } catch (const std::exception& e) {
        std::cerr << "SoundManager::loadConfig: Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }

    if (outData.contains("mixer") && outData["mixer"].is_object()) {
        const auto& mixer = outData["mixer"];
        frequency = mixer.value("frequency", frequency);
        channels = mixer.value("channels", channels);
        chunkSize = mixer.value("chunkSize", chunkSize);
        allocatedChannels = mixer.value("channelCount", allocatedChannels);

        std::string formatStr = mixer.value("format", "AUDIO_S16LSB");
        if (formatStr == "AUDIO_S16LSB") {
            format = AUDIO_S16LSB;
        } else if (formatStr == "AUDIO_S16MSB") {
            format = AUDIO_S16MSB;
        } else if (formatStr == "AUDIO_F32SYS") {
            format = AUDIO_F32SYS;
        } else {
            std::cerr << "SoundManager::loadConfig: Unknown audio format '" << formatStr
                      << "', defaulting to AUDIO_S16LSB" << std::endl;
            format = AUDIO_S16LSB;
        }
    }
    if (outData.contains("attenuation") && outData["attenuation"].is_object()) {
        const auto& attenuation = outData["attenuation"];
        spatialAttenuationEnabled = attenuation.value("enabled", spatialAttenuationEnabled);
        maxOffscreenDistance = attenuation.value("maxOffscreenDistance", maxOffscreenDistance);
        if (maxOffscreenDistance < 0.0f) {
            maxOffscreenDistance = 0.0f;
        }
    }
    return true;
}

void SoundManager::loadCollections(const nlohmann::json& data) {
    if (!data.is_object()) {
        std::cerr << "SoundManager::loadCollections: Expected object" << std::endl;
        return;
    }

    for (const auto& [collectionName, array] : data.items()) {
        if (!array.is_array()) {
            std::cerr << "SoundManager::loadCollections: Collection '" << collectionName << "' must be an array" << std::endl;
            continue;
        }

        SoundCollection collection;
        for (const auto& entry : array) {
            if (entry.is_string()) {
                SoundEntry sound;
                sound.path = entry.get<std::string>();
                sound.chunk = Mix_LoadWAV(sound.path.c_str());
                if (!sound.chunk) {
                    std::cerr << "SoundManager::loadCollections: Failed to load '" << sound.path
                              << "': " << Mix_GetError() << std::endl;
                    continue;
                }
                if (sound.volume >= 0) {
                    Mix_VolumeChunk(sound.chunk, sound.volume);
                }
                collection.entries.push_back(sound);
                continue;
            }

            if (!entry.is_object()) {
                std::cerr << "SoundManager::loadCollections: Invalid entry format in '" << collectionName << "'" << std::endl;
                continue;
            }

            SoundEntry sound;
            sound.path = entry.value("path", "");
            sound.volume = entry.value("volume", -1);

            if (sound.path.empty()) {
                std::cerr << "SoundManager::loadCollections: Missing path in collection '" << collectionName << "'" << std::endl;
                continue;
            }

            sound.chunk = Mix_LoadWAV(sound.path.c_str());
            if (!sound.chunk) {
                std::cerr << "SoundManager::loadCollections: Failed to load '" << sound.path
                          << "': " << Mix_GetError() << std::endl;
                continue;
            } else if (sound.volume >= 0) {
                Mix_VolumeChunk(sound.chunk, sound.volume);
            }

            collection.entries.push_back(sound);
        }

        if (!collection.entries.empty()) {
            collections[collectionName] = std::move(collection);
        } else {
            std::cerr << "SoundManager::loadCollections: Collection '" << collectionName << "' has no valid sounds" << std::endl;
        }
    }
}

void SoundManager::loadImpactRules(const nlohmann::json& data) {
    if (!data.is_object()) {
        std::cerr << "SoundManager::loadImpactRules: Expected object" << std::endl;
        return;
    }

    if (data.contains("thresholds") && data["thresholds"].is_object()) {
        const auto& thresholds = data["thresholds"];
        softThreshold = thresholds.value("soft", softThreshold);
        mediumThreshold = thresholds.value("medium", mediumThreshold);
        hardThreshold = thresholds.value("hard", hardThreshold);

        if (softThreshold > mediumThreshold) {
            mediumThreshold = softThreshold;
        }
        if (mediumThreshold > hardThreshold) {
            hardThreshold = mediumThreshold;
        }
    }

    if (data.contains("default") && data["default"].is_object()) {
        defaultImpactCollections.soft = data["default"].value("soft", defaultImpactCollections.soft);
        defaultImpactCollections.medium = data["default"].value("medium", defaultImpactCollections.medium);
        defaultImpactCollections.hard = data["default"].value("hard", defaultImpactCollections.hard);
    }

    if (data.contains("materialPairs") && data["materialPairs"].is_array()) {
        for (const auto& entry : data["materialPairs"]) {
            if (!entry.is_object()) {
                continue;
            }

            if (!entry.contains("materials") || !entry["materials"].is_array() ||
                entry["materials"].size() != 2) {
                std::cerr << "SoundManager::loadImpactRules: materialPairs entry missing materials array" << std::endl;
                continue;
            }

            std::string materialA = entry["materials"][0].get<std::string>();
            std::string materialB = entry["materials"][1].get<std::string>();
            std::string key = makeMaterialKey(materialA, materialB);

            ImpactCollections collectionsEntry;
            if (entry.contains("collections") && entry["collections"].is_object()) {
                const auto& collectionsObj = entry["collections"];
                collectionsEntry.soft = collectionsObj.value("soft", "");
                collectionsEntry.medium = collectionsObj.value("medium", "");
                collectionsEntry.hard = collectionsObj.value("hard", "");
            } else {
                collectionsEntry.soft = entry.value("soft", "");
                collectionsEntry.medium = entry.value("medium", "");
                collectionsEntry.hard = entry.value("hard", "");
            }

            impactRules[key] = collectionsEntry;

            // Ensure the materials exist in the physics material library
            if (materialA != "*") {
                PhysicsMaterialLibrary::getMaterialId(materialA);
            }
            if (materialB != "*") {
                PhysicsMaterialLibrary::getMaterialId(materialB);
            }
        }
    }
}

void SoundManager::playCollection(const std::string& collectionName, float baseVolume, int channel) {
    std::scoped_lock lock(mutex);

    if (!initialized) {
        return;
    }

    playCollectionInternal(collectionName, baseVolume, std::nullopt, channel);
}

void SoundManager::playCollectionAt(const std::string& collectionName, float worldX, float worldY, float baseVolume, int channel) {
    std::scoped_lock lock(mutex);

    if (!initialized) {
        return;
    }

    playCollectionInternal(collectionName, baseVolume, std::make_optional(std::make_pair(worldX, worldY)), channel);
}

void SoundManager::playImpactSound(const CollisionImpact& impact) {
    std::scoped_lock lock(mutex);

    if (!initialized) {
        return;
    }

    std::string materialA = PhysicsMaterialLibrary::getMaterialName(impact.materialIdA);
    std::string materialB = PhysicsMaterialLibrary::getMaterialName(impact.materialIdB);
    auto* rule = resolveImpactRule(materialA, materialB);

    const std::string intensity = classifyIntensity(std::max(impact.approachSpeed, 0.0f));
    std::string collectionName;

    if (rule) {
        if (intensity == "hard" && !rule->hard.empty()) {
            collectionName = rule->hard;
        } else if (intensity == "medium" && !rule->medium.empty()) {
            collectionName = rule->medium;
        } else if (intensity == "soft" && !rule->soft.empty()) {
            collectionName = rule->soft;
        }
    }

    if (collectionName.empty()) {
        if (intensity == "hard") {
            collectionName = defaultImpactCollections.hard;
        } else if (intensity == "medium") {
            collectionName = defaultImpactCollections.medium;
        } else {
            collectionName = defaultImpactCollections.soft;
        }
    }

    if (collectionName.empty()) {
        return;
    }

    float baseVolume = 1.0f;
    if (intensity == "soft") {
        baseVolume = 0.6f;
    } else if (intensity == "medium") {
        baseVolume = 0.8f;
    }

    float worldX = impact.point.x * Engine::METERS_TO_PIXELS;
    float worldY = impact.point.y * Engine::METERS_TO_PIXELS;

    playCollectionInternal(collectionName, baseVolume, std::make_optional(std::make_pair(worldX, worldY)), -1);
}

const SoundManager::ImpactCollections* SoundManager::resolveImpactRule(
    const std::string& materialA,
    const std::string& materialB) const {
    std::string key = makeMaterialKey(materialA, materialB);

    auto exact = impactRules.find(key);
    if (exact != impactRules.end()) {
        return &exact->second;
    }

    std::string wildcardA = makeMaterialKey(materialA, "*");
    auto wildA = impactRules.find(wildcardA);
    if (wildA != impactRules.end()) {
        return &wildA->second;
    }

    std::string wildcardB = makeMaterialKey(materialB, "*");
    auto wildB = impactRules.find(wildcardB);
    if (wildB != impactRules.end()) {
        return &wildB->second;
    }

    return nullptr;
}

std::string SoundManager::classifyIntensity(float approachSpeed) const {
    if (approachSpeed >= hardThreshold) {
        return "hard";
    }
    if (approachSpeed >= mediumThreshold) {
        return "medium";
    }
    if (approachSpeed >= softThreshold) {
        return "soft";
    }
    return "soft";
}

Mix_Chunk* SoundManager::pickChunkForCollection(SoundCollection& collection) {
    if (collection.entries.empty()) {
        return nullptr;
    }

    if (collection.entries.size() == 1) {
        return collection.entries[0].chunk;
    }

    std::uniform_int_distribution<int> dist(0, static_cast<int>(collection.entries.size() - 1));
    int index = dist(rng);

    // Try to avoid repeating the same clip twice in a row
    if (collection.entries.size() > 1 && index == collection.lastIndex) {
        index = (index + 1) % static_cast<int>(collection.entries.size());
    }

    collection.lastIndex = index;
    return collection.entries[index].chunk;
}

void SoundManager::playCollectionInternal(const std::string& collectionName,
                                          float baseVolume,
                                          std::optional<std::pair<float, float>> worldPosition,
                                          int channel) {
    auto it = collections.find(collectionName);
    if (it == collections.end()) {
        std::cerr << "SoundManager::playCollection: Unknown collection '" << collectionName << "'" << std::endl;
        return;
    }

    Mix_Chunk* chunk = pickChunkForCollection(it->second);
    if (!chunk) {
        return;
    }

    float clampedBaseVolume = std::clamp(baseVolume, 0.0f, 1.0f);
    if (clampedBaseVolume <= 0.0f) {
        return;
    }

    float attenuation = 1.0f;
    if (worldPosition.has_value()) {
        attenuation = computeSpatialAttenuation(worldPosition->first, worldPosition->second);
        if (attenuation <= 0.0f) {
            return;
        }
    }

    float finalVolume = std::clamp(clampedBaseVolume * attenuation, 0.0f, 1.0f);
    if (finalVolume <= 0.0f) {
        return;
    }

    int actualChannel = Mix_PlayChannel(channel, chunk, 0);
    if (actualChannel < 0) {
        std::cerr << "SoundManager::playCollection: Mix_PlayChannel failed: " << Mix_GetError() << std::endl;
        return;
    }

    int mixVolume = static_cast<int>(std::round(finalVolume * static_cast<float>(MIX_MAX_VOLUME)));
    mixVolume = std::clamp(mixVolume, 0, MIX_MAX_VOLUME);
    Mix_Volume(actualChannel, mixVolume);
}

float SoundManager::computeSpatialAttenuation(float worldX, float worldY) const {
    if (!spatialAttenuationEnabled || engine == nullptr) {
        return 1.0f;
    }

    const auto& camera = engine->getCameraState();
    float minX = camera.viewMinX;
    float maxX = camera.viewMinX + camera.viewWidth;
    float minY = camera.viewMinY;
    float maxY = camera.viewMinY + camera.viewHeight;

    float dx = 0.0f;
    if (worldX < minX) {
        dx = minX - worldX;
    } else if (worldX > maxX) {
        dx = worldX - maxX;
    }

    float dy = 0.0f;
    if (worldY < minY) {
        dy = minY - worldY;
    } else if (worldY > maxY) {
        dy = worldY - maxY;
    }

    float offscreenDistance = std::sqrt(dx * dx + dy * dy);
    if (offscreenDistance <= 0.0f) {
        return 1.0f;
    }

    if (maxOffscreenDistance <= 0.0f || offscreenDistance >= maxOffscreenDistance) {
        return 0.0f;
    }

    float normalized = offscreenDistance / maxOffscreenDistance;
    float attenuation = 1.0f - normalized;
    return std::clamp(attenuation, 0.0f, 1.0f);
}


