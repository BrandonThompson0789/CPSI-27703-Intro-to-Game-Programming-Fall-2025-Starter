#pragma once

#include <functional>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

namespace level_editor {

class SchemaService;
class AssetCache;

class ToolbarPanel {
public:
    struct Callbacks {
        std::function<void()> onNew;
        std::function<void()> onLoad;
        std::function<void()> onSave;
        std::function<void(const std::string& templateId)> onTemplateSelected;
        std::function<void(const std::string& templateId)> onApplyTemplate;
        std::function<void(const std::string& templateId)> onEditTemplate;
        std::function<void(const std::string& templateId, int screenX, int screenY)> onBeginDrag;
    };

    ToolbarPanel();
    void Initialize(SDL_Renderer* renderer,
                    TTF_Font* font,
                    const SchemaService* schemaService,
                    AssetCache* assetCache);
    void SetCallbacks(Callbacks callbacks);
    void Render(int width, int height);
    bool HandleEvent(const SDL_Event& event, int width, int height);
    void ReloadTemplates();
    const std::string& GetSelectedTemplate() const { return selectedTemplate_; }

private:
    void RenderButtons(int width);
    void RenderTemplateGrid(int width, int height);
    bool HandleMouseButtonEvent(const SDL_Event& event, int width, int height);
    void DrawText(const std::string& text, int x, int y, SDL_Color color);
    void RefreshTemplates();
    struct ButtonDef {
        std::string label;
        std::function<void()> callback;
    };
    std::vector<ButtonDef> BuildButtonDefs() const;
    int LayoutButtons(int width,
                      const std::vector<ButtonDef>& buttons,
                      const std::function<void(const ButtonDef&, const SDL_Rect&)>& visitor) const;

    SDL_Renderer* renderer_{nullptr};
    TTF_Font* font_{nullptr};
    const SchemaService* schemaService_{nullptr};
    AssetCache* assetCache_{nullptr};
    Callbacks callbacks_;
    std::vector<std::string> templateIds_;
    std::string selectedTemplate_;
    int buttonAreaBottom_{0};
};

}  // namespace level_editor

