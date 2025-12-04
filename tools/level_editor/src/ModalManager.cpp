#include "level_editor/ModalManager.h"

namespace level_editor {

void ModalManager::RegisterModal(const ModalId& id, ModalFactory factory) {
    factories_[id] = std::move(factory);
}

void ModalManager::OpenModal(const ModalId& id, const ModalContext& context) {
    auto it = factories_.find(id);
    if (it == factories_.end()) {
        return;
    }
    it->second(context);
    openState_[id] = true;
}

bool ModalManager::IsOpen(const ModalId& id) const {
    auto it = openState_.find(id);
    if (it == openState_.end()) {
        return false;
    }
    return it->second;
}

}  // namespace level_editor

