#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace level_editor {

class LevelState;

using ModalId = std::string;

struct ModalContext {
    LevelState* levelState{nullptr};
};

using ModalFactory = std::function<void(const ModalContext&)>;

class ModalManager {
public:
    void RegisterModal(const ModalId& id, ModalFactory factory);
    void OpenModal(const ModalId& id, const ModalContext& context);
    bool IsOpen(const ModalId& id) const;

private:
    std::unordered_map<ModalId, ModalFactory> factories_;
    std::unordered_map<ModalId, bool> openState_;
};

}  // namespace level_editor

