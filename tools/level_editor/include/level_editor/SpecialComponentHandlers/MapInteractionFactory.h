#pragma once

#include <memory>

#include "level_editor/MapInteractionSession.h"

namespace level_editor {

std::unique_ptr<MapInteractionSession> CreateMapInteractionSession(MapInteractionType type, const nlohmann::json& payload);

}  // namespace level_editor



