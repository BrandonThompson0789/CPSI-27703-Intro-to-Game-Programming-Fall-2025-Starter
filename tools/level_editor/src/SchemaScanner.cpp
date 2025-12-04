#include "level_editor/SchemaScanner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>

namespace level_editor {

namespace {

std::string Trim(std::string value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
    return value;
}

bool IsBooleanLiteral(const std::string& expr) {
    const auto lower = Trim(expr);
    return lower == "true" || lower == "false";
}

bool IsNumericLiteral(const std::string& expr) {
    const auto trimmed = Trim(expr);
    if (trimmed.empty()) {
        return false;
    }
    bool sawDigit = false;
    for (char c : trimmed) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            sawDigit = true;
            continue;
        }
        if (c == '.' || c == '-' || c == '+' || c == 'f' || c == 'F') {
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        return false;
    }
    return sawDigit;
}

FieldType InferTypeFromExpression(const std::string& expr) {
    const auto trimmed = Trim(expr);
    if (trimmed.empty()) {
        return FieldType::kString;
    }
    if (!trimmed.empty() && trimmed.front() == '"' && trimmed.back() == '"') {
        return FieldType::kString;
    }
    if (IsBooleanLiteral(trimmed)) {
        return FieldType::kBoolean;
    }
    if (IsNumericLiteral(trimmed)) {
        return FieldType::kNumber;
    }
    // Heuristics for strings
    std::string lowered = trimmed;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    if (lowered.find("string") != std::string::npos || lowered.find("sprite") != std::string::npos ||
        lowered.find("name") != std::string::npos || lowered.find("sound") != std::string::npos) {
        return FieldType::kString;
    }
    // Default to number when we see common numeric hints.
    if (lowered.find("pos") != std::string::npos || lowered.find("vel") != std::string::npos ||
        lowered.find("speed") != std::string::npos || lowered.find("width") != std::string::npos ||
        lowered.find("height") != std::string::npos || lowered.find("radius") != std::string::npos ||
        lowered.find("angle") != std::string::npos || lowered.find("scale") != std::string::npos) {
        return FieldType::kNumber;
    }
    return FieldType::kString;
}

void RegisterField(SchemaCatalog& catalog, const std::string& componentType, const std::string& fieldName, FieldType type) {
    if (fieldName.empty() || componentType.empty()) {
        return;
    }
    auto& descriptor = catalog.components[componentType];
    if (descriptor.type.empty()) {
        descriptor.type = componentType;
        descriptor.displayName = componentType;
    }

    auto it = std::find_if(descriptor.fields.begin(), descriptor.fields.end(),
                           [&](const FieldDescriptor& existing) { return existing.name == fieldName; });
    if (it != descriptor.fields.end()) {
        if (it->type == FieldType::kString && type != FieldType::kString) {
            it->type = type;
        }
        return;
    }

    FieldDescriptor field;
    field.name = fieldName;
    field.label = fieldName;
    field.type = type;
    descriptor.fields.push_back(field);
}

std::vector<std::filesystem::path> ResolveRoots(const std::vector<std::string>& roots) {
    std::vector<std::filesystem::path> resolved;
    std::unordered_set<std::string> seen;
    auto cwd = std::filesystem::current_path();

    for (const auto& root : roots) {
        std::vector<std::filesystem::path> candidates;
        candidates.emplace_back(root);
        candidates.emplace_back(cwd / root);

        auto parent = cwd;
        for (int i = 0; i < 4; ++i) {
            parent = parent.parent_path();
            if (parent.empty()) {
                break;
            }
            candidates.emplace_back(parent / root);
        }

        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (candidate.empty() || !std::filesystem::exists(candidate, ec)) {
                continue;
            }
            const auto canonical = std::filesystem::weakly_canonical(candidate, ec);
            if (ec) {
                continue;
            }
            auto key = canonical.string();
            if (seen.insert(key).second) {
                resolved.push_back(canonical);
            }
            break;
        }
    }

    return resolved;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

}  // namespace

void SchemaScanner::PopulateFromCode(const std::vector<std::string>& searchRoots, SchemaCatalog& catalog) {
    auto resolvedRoots = ResolveRoots(searchRoots);
    if (resolvedRoots.empty()) {
        return;
    }

    for (const auto& root : resolvedRoots) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() != ".cpp") {
                continue;
            }
            const auto stem = entry.path().stem().string();
            if (stem.find("Component") == std::string::npos) {
                continue;
            }
            ScanFile(stem, entry.path().string(), catalog);
        }
    }
}

void SchemaScanner::ScanFile(const std::string& componentType, const std::string& path, SchemaCatalog& catalog) {
    const auto content = ReadFile(path);
    if (content.empty()) {
        return;
    }

    static const std::regex valueRegex(R"regex(value\("([A-Za-z0-9_]+)"\s*,\s*([^)\n]+)\))regex");
    static const std::regex assignmentRegex(R"regex(j\["([A-Za-z0-9_]+)"\]\s*=\s*([^;]+);)regex");
    static const std::regex containsRegex(R"regex(contains\("([A-Za-z0-9_]+)"\))regex");

    for (std::sregex_iterator it(content.begin(), content.end(), valueRegex), end; it != end; ++it) {
        const auto field = (*it)[1].str();
        const auto expr = (*it)[2].str();
        RegisterField(catalog, componentType, field, InferTypeFromExpression(expr));
    }

    for (std::sregex_iterator it(content.begin(), content.end(), assignmentRegex), end; it != end; ++it) {
        const auto field = (*it)[1].str();
        const auto expr = (*it)[2].str();
        RegisterField(catalog, componentType, field, InferTypeFromExpression(expr));
    }

    for (std::sregex_iterator it(content.begin(), content.end(), containsRegex), end; it != end; ++it) {
        const auto field = (*it)[1].str();
        RegisterField(catalog, componentType, field, FieldType::kString);
    }
}

}  // namespace level_editor

