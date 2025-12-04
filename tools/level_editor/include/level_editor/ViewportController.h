#pragma once

#include <memory>

#include "level_editor/CameraState.h"
#include "level_editor/CameraController.h"

struct SDL_Renderer;

namespace level_editor {

class ITool;
class LevelState;
class SchemaService;
class AssetCache;

enum class MouseButton {
    kNone,
    kLeft,
    kMiddle,
    kRight
};

enum class Key {
    kUnknown,
    kW,
    kA,
    kS,
    kD,
    kEscape
};

struct InputEvent {
    enum class Type {
        kNone,
        kMouseDown,
        kMouseUp,
        kMouseMove,
        kKeyDown,
        kKeyUp,
        kScroll
    };

    Type type{Type::kNone};
    MouseButton mouseButton{MouseButton::kNone};
    Key key{Key::kUnknown};
    int x{0};
    int y{0};
    int delta{0};
};

struct RenderContext {
    int viewportWidth{0};
    int viewportHeight{0};
    SDL_Renderer* renderer{nullptr};
    int offsetX{0};
    int offsetY{0};
};

class ViewportController {
public:
    ViewportController();

    void HandleInput(const InputEvent& event);
    void Update(float deltaSeconds);
    void Render(const RenderContext& context);

    void SetTool(std::shared_ptr<ITool> tool);
    void SetLevelState(LevelState* levelState);
    void SetAssetCache(AssetCache* assetCache);
    void SetSchemaService(const SchemaService* schemaService);

    const CameraState& GetCamera() const;
    void SetCamera(const CameraState& state);
    void OnViewportResized(int width, int height);
    std::pair<float, float> ScreenToWorld(float screenX, float screenY) const;
    std::pair<float, float> WorldToScreen(float worldX, float worldY) const;

private:
    void DrawBackgrounds(const RenderContext& context) const;
    void DrawGrid(const RenderContext& context) const;
    void DrawObjects(const RenderContext& context) const;
    void DrawSelection(const RenderContext& context) const;
    std::pair<float, float> BackgroundToScreen(float worldX, float worldY, float parallaxX, float parallaxY) const;

    LevelState* levelState_{nullptr};
    AssetCache* assetCache_{nullptr};
    const SchemaService* schemaService_{nullptr};
    CameraController cameraController_;
    CameraInputState cameraInputState_;
    CameraState camera_;
    std::shared_ptr<ITool> activeTool_;
    bool rightMouseDown_{false};
    int lastMouseX_{0};
    int lastMouseY_{0};
    int viewportWidth_{1280};
    int viewportHeight_{720};
    int viewportOffsetX_{0};
    int viewportOffsetY_{0};
};

}  // namespace level_editor

