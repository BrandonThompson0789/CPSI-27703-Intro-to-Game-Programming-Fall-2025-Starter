#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "level_editor/ViewportController.h"
#include "level_editor/MapInteractionSession.h"

namespace level_editor {

class LevelState;
class SchemaService;
class AssetCache;

struct ToolContext {
    LevelState* levelState{nullptr};
    ViewportController* viewport{nullptr};
    const SchemaService* schemaService{nullptr};
    AssetCache* assetCache{nullptr};
};

enum class ToolId {
    kNone,
    kSelection,
    kPlacement,
    kMoveRotate,
    kResize,
    kComponentEdit
};

struct ToolIdHash {
    std::size_t operator()(ToolId id) const noexcept { return static_cast<std::size_t>(id); }
};

enum class BoundsShape {
    kBox,
    kCircle
};

struct BoundsInfo {
    std::string name;
    float centerX{0.0f};
    float centerY{0.0f};
    float width{0.0f};
    float height{0.0f};
    float radius{0.0f};
    float angleDegrees{0.0f};
    BoundsShape shape{BoundsShape::kBox};
};

namespace tool_utils {

bool ComputeBounds(const nlohmann::json& object,
                   BoundsInfo& out,
                   const SchemaService* schemaService,
                   AssetCache* assetCache);

bool PointInBounds(const BoundsInfo& bounds, float worldX, float worldY);

std::string PickObject(LevelState* state,
                       float worldX,
                       float worldY,
                       const SchemaService* schemaService,
                       AssetCache* assetCache);

}  // namespace tool_utils

class ITool {
public:
    virtual ~ITool() = default;
    virtual void OnActivated(const ToolContext& ctx);
    virtual void OnDeactivated(const ToolContext& ctx);
    virtual void OnInput(const ToolContext& ctx, const InputEvent& event);
    virtual void OnRender(const ToolContext& ctx, const RenderContext& context);
};

class SelectionTool : public ITool {};
class PlacementTool : public ITool {
public:
    PlacementTool();
    void SetSchemaService(const SchemaService* schemaService);
    void SetLevelState(LevelState* levelState);
    void SetActiveTemplate(std::string templateId);
    bool PlaceObjectAt(float worldX, float worldY);

    void OnInput(const ToolContext& ctx, const InputEvent& event) override;

private:
    bool PlaceInternal(float worldX, float worldY);

    const SchemaService* schemaService_{nullptr};
    LevelState* levelState_{nullptr};
    std::string activeTemplateId_;
};

class MoveRotateTool : public ITool {
public:
    MoveRotateTool();

    void SetSchemaService(const SchemaService* schemaService);
    void SetLevelState(LevelState* levelState);
    void SetAssetCache(AssetCache* assetCache);

    void OnInput(const ToolContext& ctx, const InputEvent& event) override;
    void OnRender(const ToolContext& ctx, const RenderContext& context) override;

private:
    void ApplyPosition(LevelState* state, const std::string& objectName, float newX, float newY);
    float ReadBodyAngle(const nlohmann::json& object) const;
    void ApplyAngle(LevelState* state, const std::string& objectName, float newAngleDegrees);
    std::pair<float, float> RotationHandleWorldPos(const BoundsInfo& bounds, float zoom) const;

    const SchemaService* schemaService_{nullptr};
    LevelState* levelState_{nullptr};
    AssetCache* assetCache_{nullptr};

    bool dragging_{false};
    bool rotating_{false};
    std::string activeObjectName_;
    float dragOffsetX_{0.0f};
    float dragOffsetY_{0.0f};
    float initialMouseAngleRad_{0.0f};
    float initialBodyAngleDeg_{0.0f};
};

class ResizeTool : public ITool {
public:
    enum class Handle {
        kNone,
        kNorth,
        kSouth,
        kEast,
        kWest,
        kNorthEast,
        kNorthWest,
        kSouthEast,
        kSouthWest
    };

    ResizeTool();

    void SetSchemaService(const SchemaService* schemaService);
    void SetLevelState(LevelState* levelState);
    void SetAssetCache(AssetCache* assetCache);

    void OnInput(const ToolContext& ctx, const InputEvent& event) override;
    void OnRender(const ToolContext& ctx, const RenderContext& context) override;

private:
    struct DragState {
        bool active{false};
        Handle handle{Handle::kNone};
        std::string objectName;
        int dirX{0};
        int dirY{0};
        bool maintainAspect{false};
    };

    void BeginDrag(const ToolContext& ctx, const InputEvent& event);
    void UpdateDrag(const ToolContext& ctx, const InputEvent& event);
    void EndDrag();
    Handle HitTestHandles(const ToolContext& ctx, const BoundsInfo& bounds, int screenX, int screenY) const;
    void ApplyResize(LevelState* state, const BoundsInfo& bounds, Handle handle, float worldX, float worldY);
    void DrawHandles(const ToolContext& ctx, const RenderContext& context, const BoundsInfo& bounds) const;

    const SchemaService* schemaService_{nullptr};
    LevelState* levelState_{nullptr};
    AssetCache* assetCache_{nullptr};
    DragState drag_;
};

class ComponentEditTool : public ITool {
public:
    ComponentEditTool();

    void BeginInteraction(MapInteractionRequest request, std::function<void()> onFinished);
    void CancelInteraction();
    bool HasActiveInteraction() const { return session_ != nullptr; }

    void OnActivated(const ToolContext& ctx) override;
    void OnDeactivated(const ToolContext& ctx) override;
    void OnInput(const ToolContext& ctx, const InputEvent& event) override;
    void OnRender(const ToolContext& ctx, const RenderContext& context) override;

private:
    void CompleteInteraction(bool cancelled);

    std::unique_ptr<MapInteractionSession> session_;
    MapInteractionRequest request_;
    std::function<void()> finishedCallback_;
    bool completing_{false};
};

using ToolPtr = std::shared_ptr<ITool>;

class ToolController {
public:
    void SetViewport(ViewportController* viewport);
    void RegisterTool(ToolId id, ToolPtr tool);
    bool ActivateTool(ToolId id);
    ToolId GetActiveTool() const { return activeTool_; }
    ToolPtr GetTool(ToolId id) const;

private:
    ViewportController* viewport_{nullptr};
    ToolId activeTool_{ToolId::kNone};
    std::unordered_map<ToolId, ToolPtr, ToolIdHash> tools_;
};

}  // namespace level_editor

