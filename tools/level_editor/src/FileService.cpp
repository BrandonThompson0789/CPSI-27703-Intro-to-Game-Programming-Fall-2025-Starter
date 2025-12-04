#include "level_editor/FileService.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "level_editor/SchemaService.h"

namespace level_editor {

namespace {

namespace fs = std::filesystem;

std::string SanitizeFileName(std::string value) {
    if (value.empty()) {
        return "level";
    }
    for (auto& ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
            ch = '_';
        }
    }
    return value;
}

}  // namespace

FileService::FileService(const SchemaService* schemaService, FileServiceConfig config)
    : config_(std::move(config)), schemaService_(schemaService) {}

LevelDocument FileService::NewLevel() const {
    LevelDocument doc;
    doc.title = "Untitled Level";
    doc.order = 0;
    doc.data = nlohmann::json::object();
    doc.data["title"] = doc.title;
    doc.data["order"] = doc.order;
    doc.data["background"] = {{"layers", nlohmann::json::array()}};
    doc.data["objects"] = nlohmann::json::array();
    return doc;
}

FileOperationResult FileService::LoadLevel(const std::string& path) const {
    FileOperationResult result;
    const auto resolved = ResolvePath(path);
    if (resolved.empty()) {
        result.message = "Level path is empty or invalid.";
        return result;
    }

    std::ifstream stream(resolved);
    if (!stream.is_open()) {
        result.message = "Failed to open level file: " + resolved.string();
        return result;
    }

    nlohmann::json data;
    try {
        stream >> data;
    } catch (const std::exception& ex) {
        result.message = std::string("Failed to parse JSON: ") + ex.what();
        return result;
    }

    if (!data.is_object()) {
        result.message = "Level JSON must be an object.";
        return result;
    }

    LevelDocument document;
    document.sourcePath = resolved.string();
    document.data = data;
    document.title = data.value("title", std::string("Untitled Level"));
    document.order = data.value("order", 0);

    result.document = document;
    result.success = true;

    if (schemaService_) {
        SchemaValidator validator(schemaService_->GetCatalog());
        if (data.contains("objects") && data["objects"].is_array()) {
            for (std::size_t i = 0; i < data["objects"].size(); ++i) {
                const auto validation = validator.ValidateObjectDefinition(data["objects"][i]);
                if (!validation.ok && result.message.empty()) {
                    result.message = "Validation issues found in level.";
                }
                result.issues.insert(result.issues.end(), validation.issues.begin(), validation.issues.end());
            }
        }
    }

    if (result.message.empty()) {
        result.message = "Level loaded.";
    }

    return result;
}

bool FileService::SaveLevel(const std::string& path, const LevelDocument& document) const {
    auto resolved = ResolvePath(path);
    if (resolved.empty()) {
        if (!document.sourcePath.empty()) {
            resolved = ResolvePath(document.sourcePath);
        } else {
            resolved = DefaultLevelPath(document.title);
        }
    }

    if (resolved.empty()) {
        return false;
    }

    if (!EnsureDirectoryExists(resolved)) {
        return false;
    }

    nlohmann::json data = document.data;
    data["title"] = document.title;
    data["order"] = document.order;

    std::ofstream stream(resolved);
    if (!stream.is_open()) {
        return false;
    }

    stream << data.dump(4);
    return stream.good();
}

bool FileService::ConfirmSaveIfDirty(bool dirtyFlag, const std::function<void()>& saveCallback) const {
    if (!dirtyFlag) {
        return true;
    }
    if (saveCallback) {
        saveCallback();
    }
    return true;
}

fs::path FileService::ResolvePath(const std::string& path) const {
    if (path.empty()) {
        return {};
    }

    fs::path candidate(path);
    if (!candidate.is_absolute()) {
        fs::path configPath = config_.levelsDirectory.empty() ? fs::path{} : fs::path(config_.levelsDirectory);
        fs::path base = configPath.empty()
                            ? fs::current_path()
                            : (configPath.is_absolute() ? configPath : fs::current_path() / configPath);

        if (!configPath.empty()) {
            const std::string configStr = configPath.generic_string();
            const std::string candidateStr = candidate.generic_string();
            if (candidateStr.rfind(configStr, 0) != 0) {
                candidate = base / candidate;
            }
        } else {
            candidate = base / candidate;
        }
    }

    std::error_code ec;
    auto canonical = fs::weakly_canonical(candidate, ec);
    if (ec) {
        return candidate;
    }
    return canonical;
}

fs::path FileService::DefaultLevelPath(const std::string& title) const {
    auto base = config_.levelsDirectory.empty() ? fs::current_path() : fs::path(config_.levelsDirectory);
    auto name = SanitizeFileName(title);
    return base / (name + ".json");
}

bool FileService::EnsureDirectoryExists(const fs::path& path) const {
    const auto dir = path.parent_path();
    if (dir.empty()) {
        return true;
    }
    std::error_code ec;
    if (fs::exists(dir, ec)) {
        return true;
    }
    return fs::create_directories(dir, ec);
}

}  // namespace level_editor

