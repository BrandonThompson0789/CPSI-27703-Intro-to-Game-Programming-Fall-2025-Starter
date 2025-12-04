#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "level_editor/Command.h"

namespace level_editor {

struct LevelDocument {
    std::string title{"Untitled"};
    int order{0};
    std::string sourcePath;
    nlohmann::json data{nlohmann::json::object()};
};

class LevelState {
public:
    LevelState();

    const LevelDocument& GetDocument() const;
    LevelDocument& GetDocument();
    void SetDocument(LevelDocument document);

    bool IsDirty() const;
    void ClearDirtyFlag();
    void MarkDirty();

    void PushCommand(CommandPtr command);
    bool Undo();
    bool Redo();

    void SetSelection(std::vector<std::string> selection);
    const std::vector<std::string>& GetSelection() const;
    void AppendObject(nlohmann::json object);
    std::string GenerateUniqueObjectName(const std::string& baseName) const;
    void UpdateSourcePath(const std::string& path);

private:
    void EnsureObjectNames();

    LevelDocument document_;
    std::vector<CommandPtr> undoStack_;
    std::vector<CommandPtr> redoStack_;
    std::vector<std::string> selection_;
    bool dirty_{false};
};

}  // namespace level_editor

