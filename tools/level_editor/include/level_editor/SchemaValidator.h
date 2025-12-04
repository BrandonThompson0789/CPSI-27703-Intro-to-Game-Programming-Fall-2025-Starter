#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "level_editor/SchemaTypes.h"

namespace level_editor {

class SchemaValidator {
public:
    struct Issue {
        std::string path;
        std::string message;
        bool warning{false};
    };

    struct Result {
        bool ok{true};
        std::vector<Issue> issues;
    };

    explicit SchemaValidator(const SchemaCatalog& catalog);

    Result ValidateComponentData(const std::string& componentType, const nlohmann::json& data) const;
    Result ValidateObjectDefinition(const nlohmann::json& objectData) const;

private:
    Issue MakeIssue(const std::string& path, const std::string& message, bool warning = false) const;
    bool ValidateFieldType(const FieldDescriptor& descriptor, const nlohmann::json& value, Issue& outIssue) const;

    const SchemaCatalog& catalog_;
};

}  // namespace level_editor

