#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

struct SDL_Renderer;

namespace level_editor {

struct ToolContext;
struct InputEvent;
struct RenderContext;

enum class MapInteractionType {
    kSensorBox,
    kRailPath,
    kJointAnchors,
    kSpawnerLocations
};

struct MapInteractionResult {
    MapInteractionType type;
    nlohmann::json payload;
};

struct MapInteractionRequest {
    MapInteractionType type{MapInteractionType::kSensorBox};
    nlohmann::json payload;
    std::function<void(const MapInteractionResult& result)> onComplete;
    std::function<void()> onCancelled;
};

class MapInteractionSession {
public:
    virtual ~MapInteractionSession() = default;

    virtual void OnInput(const ToolContext& ctx, const InputEvent& event) = 0;
    virtual void OnRender(const ToolContext& ctx, const RenderContext& context) = 0;
    virtual bool IsFinished() const = 0;
    virtual bool IsCancelled() const = 0;
    virtual MapInteractionResult BuildResult() const = 0;
};

}  // namespace level_editor



