#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "level_editor/SchemaTypes.h"

namespace level_editor {

class SchemaService {
public:
    SchemaService();
    explicit SchemaService(SchemaCatalog catalog);

    static SchemaCatalog LoadAll(const SchemaSources& sources);

    const TemplateDescriptor* GetTemplate(const std::string& id) const;
    const ComponentDescriptor* GetComponentDescriptor(const std::string& type) const;
    const nlohmann::json* GetTemplateComponentDefaults(const std::string& templateId, const std::string& componentType) const;

    std::vector<std::string> ListTemplateIds() const;
    std::vector<std::string> ListComponentTypes() const;
    const std::vector<std::string>& GetSpriteNames() const;
    const std::vector<std::string>& GetSoundCollections() const;
    const SchemaCatalog& GetCatalog() const { return catalog_; }
    void Reload(const SchemaSources& sources);
    void ReplaceCatalog(SchemaCatalog catalog);

private:
    SchemaCatalog catalog_;
};

}  // namespace level_editor

