#include "level_editor/SchemaValidator.h"

#include <algorithm>
#include <sstream>

namespace level_editor {

namespace {

bool IsApproxNumber(const nlohmann::json& value) {
    return value.is_number_float() || value.is_number_integer() || value.is_number_unsigned();
}

}  // namespace

SchemaValidator::SchemaValidator(const SchemaCatalog& catalog) : catalog_(catalog) {}

SchemaValidator::Issue SchemaValidator::MakeIssue(const std::string& path, const std::string& message, bool warning) const {
    return Issue{path, message, warning};
}

bool SchemaValidator::ValidateFieldType(const FieldDescriptor& descriptor, const nlohmann::json& value, Issue& outIssue) const {
    auto issuePath = "field:" + descriptor.name;
    switch (descriptor.type) {
        case FieldType::kString:
        case FieldType::kSpriteRef:
        case FieldType::kSoundCollectionRef:
        case FieldType::kObjectReference:
            if (!value.is_string()) {
                outIssue = MakeIssue(issuePath, "Expected string value");
                return false;
            }
            break;
        case FieldType::kNumber:
            if (!IsApproxNumber(value)) {
                outIssue = MakeIssue(issuePath, "Expected numeric value");
                return false;
            }
            if (descriptor.minValue && value.get<double>() < *descriptor.minValue) {
                outIssue = MakeIssue(issuePath, "Value below minimum");
                return false;
            }
            if (descriptor.maxValue && value.get<double>() > *descriptor.maxValue) {
                outIssue = MakeIssue(issuePath, "Value above maximum");
                return false;
            }
            break;
        case FieldType::kBoolean:
            if (!value.is_boolean()) {
                outIssue = MakeIssue(issuePath, "Expected boolean value");
                return false;
            }
            break;
        case FieldType::kVector2:
            if (!value.is_array() || value.size() < 2) {
                outIssue = MakeIssue(issuePath, "Expected array with [x,y]");
                return false;
            }
            if (!IsApproxNumber(value[0]) || !IsApproxNumber(value[1])) {
                outIssue = MakeIssue(issuePath, "Vector elements must be numeric");
                return false;
            }
            break;
        case FieldType::kEnum:
            if (!value.is_string()) {
                outIssue = MakeIssue(issuePath, "Expected enum string");
                return false;
            }
            if (!descriptor.enumValues.empty()) {
                const auto strValue = value.get<std::string>();
                const auto it = std::find(descriptor.enumValues.begin(), descriptor.enumValues.end(), strValue);
                if (it == descriptor.enumValues.end()) {
                    outIssue = MakeIssue(issuePath, "Enum value not recognized");
                    return false;
                }
            }
            break;
    }
    return true;
}

SchemaValidator::Result SchemaValidator::ValidateComponentData(const std::string& componentType, const nlohmann::json& data) const {
    Result result;

    const auto descriptorIt = catalog_.components.find(componentType);
    if (descriptorIt == catalog_.components.end()) {
        result.ok = false;
        result.issues.push_back(MakeIssue("component", "Unknown component type: " + componentType));
        return result;
    }

    const auto& descriptor = descriptorIt->second;
    for (const auto& field : descriptor.fields) {
        const bool hasField = data.contains(field.name);
        if (field.required && !hasField) {
            result.ok = false;
            result.issues.push_back(MakeIssue("field:" + field.name, "Required field is missing"));
            continue;
        }
        if (!hasField) {
            continue;
        }

        Issue fieldIssue;
        if (!ValidateFieldType(field, data[field.name], fieldIssue)) {
            result.ok = false;
            result.issues.push_back(fieldIssue);
        }
    }

    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto key = it.key();
        if (key == "type") {
            continue;
        }
        const bool known = std::any_of(descriptor.fields.begin(), descriptor.fields.end(),
                                       [&](const FieldDescriptor& field) { return field.name == key; });
        if (!known) {
            result.issues.push_back(MakeIssue("field:" + key, "Field not recognized", true));
        }
    }

    return result;
}

SchemaValidator::Result SchemaValidator::ValidateObjectDefinition(const nlohmann::json& objectData) const {
    Result result;

    if (!objectData.contains("template")) {
        result.ok = false;
        result.issues.push_back(MakeIssue("object", "Object missing template id"));
        return result;
    }

    const auto templateId = objectData.value("template", std::string{});
    const auto templateIt = catalog_.templates.find(templateId);
    if (templateIt == catalog_.templates.end()) {
        result.ok = false;
        result.issues.push_back(MakeIssue("object", "Template not found: " + templateId));
    }

    if (objectData.contains("components") && objectData["components"].is_array()) {
        for (std::size_t i = 0; i < objectData["components"].size(); ++i) {
            const auto& component = objectData["components"][i];
            if (!component.contains("type")) {
                result.ok = false;
                result.issues.push_back(MakeIssue("components[" + std::to_string(i) + "]", "Component missing type"));
                continue;
            }
            const auto type = component.value("type", std::string{});
            const auto componentResult = ValidateComponentData(type, component);
            if (!componentResult.ok) {
                result.ok = false;
            }
            for (const auto& issue : componentResult.issues) {
                auto scopedIssue = issue;
                scopedIssue.path = "components[" + std::to_string(i) + "]." + scopedIssue.path;
                result.issues.push_back(scopedIssue);
            }
        }
    }

    return result;
}

}  // namespace level_editor

