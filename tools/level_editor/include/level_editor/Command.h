#pragma once

#include <memory>

namespace level_editor {

struct LevelDocument;

class Command {
public:
    virtual ~Command() = default;
    virtual void Execute(LevelDocument& document) = 0;
    virtual void Undo(LevelDocument& document) = 0;
};

using CommandPtr = std::unique_ptr<Command>;

}  // namespace level_editor

