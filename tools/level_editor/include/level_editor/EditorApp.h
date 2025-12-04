#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <nlohmann/json.hpp>

#include "level_editor/AssetCache.h"
#include "level_editor/FileService.h"
#include "level_editor/LevelState.h"
#include "level_editor/ModalManager.h"
#include "level_editor/SchemaService.h"
#include "level_editor/ViewportController.h"
#include "level_editor/ToolSystem/Tool.h"
#include "level_editor/ui/ComponentEditorPanel.h"
#include "level_editor/ui/FileDialog.h"
#include "level_editor/ui/TemplateEditorModal.h"
#include "level_editor/ui/ToolbarPanel.h"

struct SDL_Window;
struct SDL_Renderer;

namespace level_editor {

class EditorApp {
public:
    EditorApp();
    int Run();

private:
    bool Initialize();
    void Shutdown();
    bool InitializeSDL();
    void ShutdownSDL();
    void ProcessEvent(const SDL_Event& event, bool& running);
    bool HandleToolbarEvent(const SDL_Event& event);
    void LoadStartupLevel();
    void HandleNewLevel();
    void OpenLoadDialog();
    bool HandleLoadLevel(const std::string& path);
    bool HandleSaveLevel(const std::string& path = "");
    void OpenTemplateEditor(const std::string& templateId);
    void HandleTemplateEditResult(const TemplateEditResult& result);
    bool LoadTemplatesFile(nlohmann::json& out) const;
    bool SaveTemplatesFile(const nlohmann::json& data) const;
    void ApplyTemplateToObjects(const std::string& templateId);
    void ActivateTool(ToolId toolId);
    void BeginTemplateDrag(const std::string& templateId, int screenX, int screenY);
    void UpdateTemplateDrag(int screenX, int screenY);
    void EndTemplateDrag(bool place, int screenX, int screenY);
    void RenderTemplateDragPreview();
    bool HandleSelectionClick(int screenX, int screenY);
    bool HandleViewportContextMenuRequest(int screenX, int screenY);
    void ShowToolMenu(int screenX, int screenY);
    bool HandleToolMenuEvent(const SDL_Event& event);
    void RenderToolMenu();
    void CloseToolMenu();
    void BeginMapInteraction(const MapInteractionRequest& request);
    void OnMapInteractionFinished();
    bool HandleComponentPanelEvent(const SDL_Event& event);
    void UpdateComponentPanelRect(int viewportWidth);
    std::string PickObjectAtScreen(int screenX, int screenY) const;
    bool IsPointInsideViewport(int screenX, int screenY) const;
    int ToolMenuEntryIndexAt(int screenX, int screenY) const;
    const nlohmann::json* GetSelectedObject() const;
    bool IsPointOnActiveHandle(int screenX, int screenY) const;
    bool IsPointNearMoveRotateHandle(const BoundsInfo& bounds, int screenX, int screenY) const;
    bool IsPointNearResizeHandle(const BoundsInfo& bounds, int screenX, int screenY) const;

    SchemaSources schemaSources_;
    SchemaService schemaService_;
    AssetCache assetCache_;
    LevelState levelState_;
    ViewportController viewport_;
    ModalManager modalManager_;
    FileService fileService_;
    ToolbarPanel toolbar_;
    FileDialog fileDialog_;
    TemplateEditorModal templateEditor_;
    ComponentEditorPanel componentPanel_;
    std::shared_ptr<SelectionTool> selectionTool_;
    std::shared_ptr<PlacementTool> placementTool_;
    std::shared_ptr<MoveRotateTool> moveRotateTool_;
    std::shared_ptr<ResizeTool> resizeTool_;
    std::shared_ptr<ComponentEditTool> componentEditTool_;
    ToolController toolController_;
    bool running_{false};
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    TTF_Font* uiFont_{nullptr};
    int windowWidth_{1600};
    int windowHeight_{900};
    const int toolbarWidth_{320};
    const int inspectorWidth_{420};
    SDL_Rect componentPanelRect_{0, 0, 0, 0};
    std::string currentLevelPath_;
    bool templateDragActive_{false};
    std::string templateDragTemplateId_;
    int templateDragX_{0};
    int templateDragY_{0};
    struct ToolMenuEntry {
        std::string label;
        ToolId toolId;
        SDL_Keycode hotkey;
    };
    bool toolMenuOpen_{false};
    SDL_Rect toolMenuRect_{0, 0, 0, 0};
    std::vector<ToolMenuEntry> toolMenuEntries_;
    int toolMenuHoverIndex_{-1};
    ToolId previousToolBeforeMap_{ToolId::kSelection};
};

}  // namespace level_editor

