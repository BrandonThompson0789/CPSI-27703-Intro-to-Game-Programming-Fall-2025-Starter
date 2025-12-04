#include "level_editor/LevelState.h"

#include <cctype>
#include <unordered_set>
#include <utility>

namespace level_editor {

LevelState::LevelState() = default;

const LevelDocument& LevelState::GetDocument() const {
    return document_;
}

LevelDocument& LevelState::GetDocument() {
    return document_;
}

void LevelState::SetDocument(LevelDocument document) {
    document_ = std::move(document);
    EnsureObjectNames();
    ClearDirtyFlag();
}

bool LevelState::IsDirty() const {
    return dirty_;
}

void LevelState::ClearDirtyFlag() {
    dirty_ = false;
}

void LevelState::MarkDirty() {
    dirty_ = true;
}

void LevelState::PushCommand(CommandPtr command) {
    if (!command) {
        return;
    }
    command->Execute(document_);
    undoStack_.push_back(std::move(command));
    redoStack_.clear();
    dirty_ = true;
}

bool LevelState::Undo() {
    if (undoStack_.empty()) {
        return false;
    }
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();
    command->Undo(document_);
    redoStack_.push_back(std::move(command));
    dirty_ = true;
    return true;
}

bool LevelState::Redo() {
    if (redoStack_.empty()) {
        return false;
    }
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();
    command->Execute(document_);
    undoStack_.push_back(std::move(command));
    dirty_ = true;
    return true;
}

void LevelState::SetSelection(std::vector<std::string> selection) {
    selection_ = std::move(selection);
}

const std::vector<std::string>& LevelState::GetSelection() const {
    return selection_;
}

void LevelState::UpdateSourcePath(const std::string& path) {
    document_.sourcePath = path;
}

void LevelState::AppendObject(nlohmann::json object) {
    auto& objects = document_.data["objects"];
    if (!objects.is_array()) {
        objects = nlohmann::json::array();
    }
    objects.push_back(std::move(object));
    dirty_ = true;
}

std::string LevelState::GenerateUniqueObjectName(const std::string& baseName) const {
    std::string sanitized = baseName;
    for (char& ch : sanitized) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            ch = '_';
        }
    }
    if (sanitized.empty()) {
        sanitized = "object";
    }

    std::unordered_set<std::string> existingNames;
    const auto& objects = document_.data["objects"];
    if (objects.is_array()) {
        for (const auto& obj : objects) {
            if (obj.contains("name") && obj["name"].is_string()) {
                existingNames.insert(obj["name"].get<std::string>());
            }
        }
    }

    std::string candidate = sanitized;
    int suffix = 1;
    while (existingNames.count(candidate) > 0) {
        candidate = sanitized + "_" + std::to_string(suffix++);
    }
    return candidate;
}

void LevelState::EnsureObjectNames() {
    auto& data = document_.data;
    auto objectsIt = data.find("objects");
    if (objectsIt == data.end() || !objectsIt->is_array()) {
        return;
    }

    std::unordered_set<std::string> existingNames;
    for (const auto& obj : *objectsIt) {
        if (obj.contains("name") && obj["name"].is_string()) {
            const auto name = obj["name"].get<std::string>();
            if (!name.empty()) {
                existingNames.insert(name);
            }
        }
    }

    for (auto& obj : *objectsIt) {
        if (obj.contains("name") && obj["name"].is_string() && !obj["name"].get<std::string>().empty()) {
            continue;
        }

        std::string baseName;
        if (obj.contains("template") && obj["template"].is_string() && !obj["template"].get<std::string>().empty()) {
            baseName = obj["template"].get<std::string>();
        } else {
            baseName = "object";
        }

        std::string candidate = baseName;
        int suffix = 1;
        while (existingNames.count(candidate) > 0) {
            candidate = baseName + "_" + std::to_string(suffix++);
        }
        obj["name"] = candidate;
        existingNames.insert(candidate);
    }
}

}  // namespace level_editor

