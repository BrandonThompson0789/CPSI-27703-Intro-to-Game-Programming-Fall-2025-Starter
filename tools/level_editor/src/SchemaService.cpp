#include "level_editor/SchemaService.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "level_editor/SchemaScanner.h"

namespace level_editor {

namespace {

using json = nlohmann::json;

json LoadJsonFile(const std::string& path) {
    if (path.empty()) {
        return json::object();
    }

    std::ifstream stream(path);
    if (!stream.is_open()) {
        std::cerr << "[SchemaService] Failed to open JSON file: " << path << '\n';
        return json::object();
    }

    json data;
    try {
        stream >> data;
    } catch (const std::exception& ex) {
        std::cerr << "[SchemaService] Failed to parse JSON (" << path << "): " << ex.what() << '\n';
        return json::object();
    }
    return data;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

FieldType InferFieldType(const std::string& fieldName, const json& value) {
    if (value.is_boolean()) {
        return FieldType::kBoolean;
    }
    if (value.is_number()) {
        return FieldType::kNumber;
    }
    if (value.is_string()) {
        auto lower = ToLower(fieldName);
        if (lower.find("sprite") != std::string::npos) {
            return FieldType::kSpriteRef;
        }
        if (lower.find("sound") != std::string::npos || lower.find("collection") != std::string::npos) {
            return FieldType::kSoundCollectionRef;
        }
        if (lower.find("target") != std::string::npos || lower.find("object") != std::string::npos) {
            return FieldType::kObjectReference;
        }
        return FieldType::kString;
    }
    if (value.is_array()) {
        if (!value.empty()) {
            const auto& first = value.front();
            if (first.is_number()) {
                return FieldType::kVector2;
            }
            if (first.is_boolean()) {
                return FieldType::kBoolean;
            }
        }
        return FieldType::kString;
    }
    if (value.is_object()) {
        return FieldType::kString;
    }
    return FieldType::kString;
}

void MergeFieldDescriptor(ComponentDescriptor& descriptor, const FieldDescriptor& field) {
    auto it = std::find_if(descriptor.fields.begin(), descriptor.fields.end(),
                           [&](const FieldDescriptor& existing) { return existing.name == field.name; });
    if (it == descriptor.fields.end()) {
        descriptor.fields.push_back(field);
    }
}

void ParseTemplates(const json& templatesJson, SchemaCatalog& catalog) {
    if (!templatesJson.is_object()) {
        return;
    }

    const auto templatesIt = templatesJson.find("templates");
    if (templatesIt == templatesJson.end() || !templatesIt->is_object()) {
        return;
    }

    for (const auto& entry : templatesIt->items()) {
        const auto& templateId = entry.key();
        const auto& templateData = entry.value();

        TemplateDescriptor descriptor;
        descriptor.id = templateId;
        descriptor.displayName = templateData.value("name", templateId);

        if (templateData.contains("components") && templateData["components"].is_array()) {
            for (const auto& component : templateData["components"]) {
                const auto type = component.value("type", std::string{});
                if (type.empty()) {
                    continue;
                }
                descriptor.defaultComponents.push_back(type);
                descriptor.componentDefaults.push_back(component);

                auto& componentDescriptor = catalog.components[type];
                if (componentDescriptor.type.empty()) {
                    componentDescriptor.type = type;
                    componentDescriptor.displayName = type;
                }

                for (const auto& field : component.items()) {
                    if (field.key() == "type") {
                        continue;
                    }
                    FieldDescriptor fieldDescriptor;
                    fieldDescriptor.name = field.key();
                    fieldDescriptor.label = field.key();
                    fieldDescriptor.type = InferFieldType(field.key(), field.value());

                    if (type == "SpriteComponent" && fieldDescriptor.name == "spriteName" && field.value().is_string()) {
                        descriptor.spriteName = field.value().get<std::string>();
                    }

                    MergeFieldDescriptor(componentDescriptor, fieldDescriptor);
                }
            }
        }

        catalog.templates[descriptor.id] = descriptor;
    }
}

void ParseSpriteData(const json& spriteJson, SchemaCatalog& catalog) {
    const auto texturesIt = spriteJson.find("textures");
    if (texturesIt == spriteJson.end() || !texturesIt->is_object()) {
        return;
    }

    std::set<std::string> spriteNames;
    for (const auto& textureEntry : texturesIt->items()) {
        const auto& textureData = textureEntry.value();
        const auto spritesIt = textureData.find("sprites");
        if (spritesIt == textureData.end() || !spritesIt->is_object()) {
            continue;
        }
        for (const auto& spriteEntry : spritesIt->items()) {
            spriteNames.insert(spriteEntry.key());
        }
    }
    catalog.spriteNames.assign(spriteNames.begin(), spriteNames.end());
}

void ParseSoundData(const json& soundJson, SchemaCatalog& catalog) {
    const auto collectionsIt = soundJson.find("collections");
    if (collectionsIt == soundJson.end() || !collectionsIt->is_object()) {
        return;
    }

    std::set<std::string> collectionNames;
    for (const auto& entry : collectionsIt->items()) {
        collectionNames.insert(entry.key());
    }
    catalog.soundCollections.assign(collectionNames.begin(), collectionNames.end());
}

}  // namespace

SchemaService::SchemaService() = default;

SchemaService::SchemaService(SchemaCatalog catalog) : catalog_(std::move(catalog)) {}

SchemaCatalog SchemaService::LoadAll(const SchemaSources& sources) {
    SchemaCatalog catalog;

    ParseTemplates(LoadJsonFile(sources.templatesJson), catalog);
    ParseSpriteData(LoadJsonFile(sources.spritesJson), catalog);
    ParseSoundData(LoadJsonFile(sources.soundsJson), catalog);

    if (!sources.additionalSearchRoots.empty()) {
        SchemaScanner::PopulateFromCode(sources.additionalSearchRoots, catalog);
    }
    // TODO(step1): Scan component/serializer code more deeply (e.g., serializer helpers) for nested structures.
    return catalog;
}

const TemplateDescriptor* SchemaService::GetTemplate(const std::string& id) const {
    auto it = catalog_.templates.find(id);
    if (it == catalog_.templates.end()) {
        return nullptr;
    }
    return &it->second;
}

const ComponentDescriptor* SchemaService::GetComponentDescriptor(const std::string& type) const {
    auto it = catalog_.components.find(type);
    if (it == catalog_.components.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> SchemaService::ListTemplateIds() const {
    std::vector<std::string> ids;
    ids.reserve(catalog_.templates.size());
    for (const auto& kvp : catalog_.templates) {
        ids.push_back(kvp.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> SchemaService::ListComponentTypes() const {
    std::vector<std::string> types;
    types.reserve(catalog_.components.size());
    for (const auto& kvp : catalog_.components) {
        types.push_back(kvp.first);
    }
    std::sort(types.begin(), types.end());
    return types;
}

const std::vector<std::string>& SchemaService::GetSpriteNames() const {
    return catalog_.spriteNames;
}

const std::vector<std::string>& SchemaService::GetSoundCollections() const {
    return catalog_.soundCollections;
}

const nlohmann::json* SchemaService::GetTemplateComponentDefaults(const std::string& templateId,
                                                                  const std::string& componentType) const {
    auto it = catalog_.templates.find(templateId);
    if (it == catalog_.templates.end()) {
        return nullptr;
    }
    for (const auto& componentJson : it->second.componentDefaults) {
        if (componentJson.value("type", std::string{}) == componentType) {
            return &componentJson;
        }
    }
    return nullptr;
}

void SchemaService::Reload(const SchemaSources& sources) {
    catalog_ = LoadAll(sources);
}

void SchemaService::ReplaceCatalog(SchemaCatalog catalog) {
    catalog_ = std::move(catalog);
}

}  // namespace level_editor

