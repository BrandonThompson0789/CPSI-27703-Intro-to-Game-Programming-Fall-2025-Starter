#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "level_editor/LevelState.h"
#include "level_editor/SchemaValidator.h"

namespace level_editor {

class SchemaService;

struct FileServiceConfig {
    std::string levelsDirectory;
};

struct FileOperationResult {
    bool success{false};
    std::string message;
    LevelDocument document;
    std::vector<SchemaValidator::Issue> issues;
};

class FileService {
public:
    explicit FileService(const SchemaService* schemaService = nullptr, FileServiceConfig config = {});

    LevelDocument NewLevel() const;
    FileOperationResult LoadLevel(const std::string& path) const;
    bool SaveLevel(const std::string& path, const LevelDocument& document) const;
    bool ConfirmSaveIfDirty(bool dirtyFlag, const std::function<void()>& saveCallback) const;

private:
    std::filesystem::path ResolvePath(const std::string& path) const;
    std::filesystem::path DefaultLevelPath(const std::string& title) const;
    bool EnsureDirectoryExists(const std::filesystem::path& path) const;

    FileServiceConfig config_;
    const SchemaService* schemaService_{nullptr};
};

}  // namespace level_editor

