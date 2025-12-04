#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

#include "level_editor/MapInteractionSession.h"
#include "level_editor/SchemaTypes.h"

namespace level_editor {

class LevelState;
class SchemaService;
class AssetCache;

class ComponentEditorPanel {
public:
    struct Callbacks {
        std::function<void(const MapInteractionRequest& request)> onRequestMapInteraction;
    };

    ComponentEditorPanel();

    void Initialize(SDL_Renderer* renderer,
                    TTF_Font* font,
                    LevelState* levelState,
                    const SchemaService* schemaService,
                    AssetCache* assetCache);
    void SetCallbacks(Callbacks callbacks);

    void Render(const SDL_Rect& bounds);
    bool HandleEvent(const SDL_Event& event, const SDL_Rect& bounds);
    void ForceRefresh();

private:
    struct TextField {
        std::string value;
        SDL_Rect rect{0, 0, 0, 0};
        size_t cursor{0};
        bool focused{false};

        void SetValue(const std::string& text);
        bool HandleMouseDown(int x, int y, TTF_Font* font);
        bool HandleTextInput(const SDL_TextInputEvent& evt);
        bool HandleKeyDown(const SDL_KeyboardEvent& evt);
        void Defocus();
    };

    enum class FieldKind {
        kString,
        kNumber,
        kBoolean,
        kEnum,
        kVector2,
        kStringList
    };

    struct FieldMeta {
        std::string id;
        std::vector<std::string> path;
        FieldKind kind{FieldKind::kString};
        FieldDescriptor descriptor;
        bool overridesTemplate{false};
        bool hasTemplateValue{false};
    };

    struct FieldState {
        TextField primary;
        TextField secondary;
        bool boolValue{false};
        bool boolPressed{false};
        bool listDirty{false};
        bool hasError{false};
        std::string errorMessage;
        bool primaryInitialized{false};
        bool secondaryInitialized{false};
    };

    struct DropdownState {
        std::string fieldId;
        SDL_Rect rect{0, 0, 0, 0};
        std::vector<std::string> options;
        bool open{false};
    };

    struct AddMenuState {
        bool open{false};
        SDL_Rect rect{0, 0, 0, 0};
        SDL_Rect anchor{0, 0, 0, 0};
        std::vector<std::string> componentTypes;
        int scroll{0};
        int visibleRows{0};
    };

    enum class HeaderFocus { kNone, kObjectName, kTemplate };

    void BeginTextInput();
    void EndTextInput();

    void SyncSelection();
    void SyncHeaderFields();
    nlohmann::json* FindSelectedObject();
    nlohmann::json* FindActiveComponent();
    void EnsureFieldCache();
    void RebuildFields();
    void CollectFields(const nlohmann::json& node,
                       const nlohmann::json* templateNode,
                       const std::string& componentKey,
                       std::vector<std::string> path,
                       std::vector<FieldMeta>& out);
    const FieldDescriptor* FindDescriptor(const std::string& componentType, const std::string& fieldName) const;
    bool ShouldSkipPath(const std::vector<std::string>& path, const std::string& componentType) const;
    FieldState& GetOrCreateState(const std::string& id, const std::string& initialValue);
    void DrawText(const std::string& text, int x, int y, SDL_Color color);
    void RenderHeader(const SDL_Rect& bounds);
    void RenderComponentList(const SDL_Rect& bounds);
    void RenderEditor(const SDL_Rect& bounds);
    void RenderField(FieldMeta& meta, FieldState& state, int x, int& y, int width);
    void RenderSpecialComponentUI(const SDL_Rect& bounds);
    void RenderAddMenu(const SDL_Rect& bounds);
    void RenderCursor(const TextField& field);

    bool HandleMouseButton(const SDL_Event& event, const SDL_Rect& bounds);
    bool HandleMouseWheel(const SDL_Event& event, const SDL_Rect& bounds);
    bool HandleTextInputEvent(const SDL_Event& event);
    bool HandleKeyEvent(const SDL_Event& event);

    void ClearFocus();
    std::string MakeFieldKey(const std::string& componentKey, const std::vector<std::string>& path) const;
    std::string PathToLabel(const std::vector<std::string>& path) const;

    void ApplyFieldChange(const FieldMeta& meta, FieldState& state);
    void ApplyVectorChange(const FieldMeta& meta, FieldState& state, bool primary);
    void ApplyListChange(const FieldMeta& meta, FieldState& state);
    void ApplyBoolChange(const FieldMeta& meta, FieldState& state);
    void ApplyEnumSelection(const FieldMeta& meta, const std::string& value);
    void ResetFieldToTemplate(const FieldMeta& meta);
    void MarkDirty();

    void RemoveComponent(const std::string& componentKey);
    void AddComponent(const std::string& type);
    void DuplicateComponent(const std::string& componentKey);

    void OpenAddMenu(const SDL_Rect& anchor);
    void CloseAddMenu();
    void ToggleDropdown(const std::string& fieldId, const SDL_Rect& anchor, std::vector<std::string> options);
    void CloseDropdown();
    bool HitTestListArea(int x, int y) const;
    bool HitTestEditorArea(int x, int y) const;

    void TriggerMapInteraction(MapInteractionType type, const nlohmann::json& payload);
    void HandleMapResult(const MapInteractionResult& result);
    void HandleSensorResult(const nlohmann::json& payload);
    void HandleRailResult(const nlohmann::json& payload);
    void HandleJointResult(const nlohmann::json& payload);
    void HandleSpawnerResult(const nlohmann::json& payload);
    const nlohmann::json* GetTemplateDefaults(const std::string& componentType);
    static FieldKind FieldKindFromDescriptor(FieldType type);
    bool HandleHeaderMouseDown(int x, int y);
    bool HandleHeaderTextInput(const SDL_TextInputEvent& event);
    bool HandleHeaderKeyEvent(const SDL_Event& event);
    void ApplyObjectNameEdit();
    void ApplyTemplateEdit();
    void ClearHeaderFocus(bool commitChanges = false);
    bool InstantiateTemplateComponent(const std::string& key);

    SDL_Renderer* renderer_{nullptr};
    TTF_Font* font_{nullptr};
    LevelState* levelState_{nullptr};
    const SchemaService* schemaService_{nullptr};
    AssetCache* assetCache_{nullptr};
    Callbacks callbacks_;

    bool textInputActive_{false};
    std::vector<std::string> selectionCache_;
    std::string activeObjectName_;
    std::string activeComponentKey_;
    int listScroll_{0};
    int editorScroll_{0};
    SDL_Rect listArea_{0, 0, 0, 0};
    SDL_Rect editorArea_{0, 0, 0, 0};
    std::vector<FieldMeta> currentFields_;
    std::unordered_map<std::string, FieldState> fieldStates_;
    std::unordered_map<std::string, bool> componentSelection_;
    std::string focusedFieldId_;
    DropdownState dropdown_;
    AddMenuState addMenu_;
    std::vector<std::pair<std::string, SDL_Rect>> componentRowRects_;
    std::vector<std::pair<std::string, SDL_Rect>> removeButtonRects_;
    std::vector<std::pair<std::string, SDL_Rect>> duplicateButtonRects_;
    std::vector<std::pair<std::string, SDL_Rect>> addMenuEntryRects_;
    std::unordered_map<std::string, SDL_Rect> fieldRects_;
    std::unordered_map<std::string, SDL_Rect> resetButtonRects_;
    std::unordered_map<std::string, SDL_Rect> boolToggleRects_;
    std::unordered_map<std::string, SDL_Rect> enumButtonRects_;
    SDL_Rect addButtonRect_{0, 0, 0, 0};
    std::vector<std::pair<std::string, SDL_Rect>> specialActionButtons_;
    TextField objectNameField_;
    TextField templateField_;
    HeaderFocus headerFocus_{HeaderFocus::kNone};
    SDL_Rect headerArea_{0, 0, 0, 0};
    std::unordered_map<std::string, nlohmann::json> templateRowData_;
};

}  // namespace level_editor


