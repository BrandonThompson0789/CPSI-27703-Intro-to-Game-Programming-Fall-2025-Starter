#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace level_editor {

enum class FieldType {
    kString,
    kNumber,
    kBoolean,
    kVector2,
    kEnum,
    kSpriteRef,
    kSoundCollectionRef,
    kObjectReference
};

struct FieldDescriptor {
    std::string name;
    FieldType type{FieldType::kString};
    std::string label;
    std::vector<std::string> enumValues;
    std::optional<double> minValue;
    std::optional<double> maxValue;
    bool required{false};
    bool allowsMapInteraction{false};
};

struct ComponentDescriptor {
    std::string type;
    std::string displayName;
    std::vector<FieldDescriptor> fields;
};

struct TemplateDescriptor {
    std::string id;
    std::string displayName;
    std::string spriteName;
    std::vector<std::string> defaultComponents;
    std::vector<nlohmann::json> componentDefaults;
};

struct SchemaCatalog {
    std::unordered_map<std::string, ComponentDescriptor> components;
    std::unordered_map<std::string, TemplateDescriptor> templates;
    std::vector<std::string> spriteNames;
    std::vector<std::string> soundCollections;
};

struct SchemaSources {
    std::string templatesJson;
    std::string spritesJson;
    std::string soundsJson;
    std::vector<std::string> additionalSearchRoots;
};

}  // namespace level_editor

