#include "level_editor/ui/ToolbarPanel.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <iostream>

#include "level_editor/AssetCache.h"
#include "level_editor/SchemaService.h"

namespace level_editor {

namespace {
constexpr int kButtonHeight = 36;
constexpr int kButtonWidth = 90;
constexpr int kButtonSpacing = 10;
constexpr int kPanelPadding = 12;
constexpr int kTemplateTileHeight = 72;
constexpr int kTemplateTilePadding = 8;
constexpr SDL_Color kTextColor{220, 220, 225, 255};
constexpr SDL_Color kHighlightColor{255, 210, 64, 255};
}  // namespace

ToolbarPanel::ToolbarPanel() = default;

void ToolbarPanel::Initialize(SDL_Renderer* renderer,
                              TTF_Font* font,
                              const SchemaService* schemaService,
                              AssetCache* assetCache) {
    renderer_ = renderer;
    font_ = font;
    schemaService_ = schemaService;
    assetCache_ = assetCache;
    RefreshTemplates();
}

void ToolbarPanel::SetCallbacks(Callbacks callbacks) {
    callbacks_ = std::move(callbacks);
    if (!selectedTemplate_.empty() && callbacks_.onTemplateSelected) {
        callbacks_.onTemplateSelected(selectedTemplate_);
    }
}

void ToolbarPanel::ReloadTemplates() {
    RefreshTemplates();
}

void ToolbarPanel::RefreshTemplates() {
    templateIds_.clear();
    if (!schemaService_) {
        return;
    }
    templateIds_ = schemaService_->ListTemplateIds();
    std::sort(templateIds_.begin(), templateIds_.end());
    if (selectedTemplate_.empty() && !templateIds_.empty()) {
        selectedTemplate_ = templateIds_.front();
    }
}

void ToolbarPanel::Render(int width, int height) {
    if (!renderer_) {
        return;
    }

    SDL_Rect background{0, 0, width, height};
    SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
    SDL_RenderFillRect(renderer_, &background);

    RenderButtons(width);
    RenderTemplateGrid(width, height);
}

void ToolbarPanel::RenderButtons(int width) {
    auto buttons = BuildButtonDefs();
    buttonAreaBottom_ = LayoutButtons(
        width,
        buttons,
        [this](const ButtonDef& button, const SDL_Rect& rect) {
            SDL_SetRenderDrawColor(renderer_, 60, 64, 72, 255);
            SDL_RenderFillRect(renderer_, &rect);
            SDL_SetRenderDrawColor(renderer_, 90, 95, 105, 255);
            SDL_RenderDrawRect(renderer_, &rect);
            DrawText(button.label, rect.x + 10, rect.y + 8, kTextColor);
        });
}

void ToolbarPanel::RenderTemplateGrid(int width, int height) {
    const int baseline = buttonAreaBottom_ > 0 ? buttonAreaBottom_ : (kPanelPadding + kButtonHeight);
    const int startY = baseline + kPanelPadding;
    int y = startY;
    int x = kPanelPadding;
    const int tileWidth = width - kPanelPadding * 2;

    SDL_Color selectedColor{70, 130, 180, 255};
    for (const auto& templateId : templateIds_) {
        SDL_Rect tile{x, y, tileWidth, kTemplateTileHeight};
        if (templateId == selectedTemplate_) {
            SDL_SetRenderDrawColor(renderer_, selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a);
        } else {
            SDL_SetRenderDrawColor(renderer_, 50, 54, 60, 255);
        }
        SDL_RenderFillRect(renderer_, &tile);
        SDL_SetRenderDrawColor(renderer_, 80, 84, 92, 255);
        SDL_RenderDrawRect(renderer_, &tile);

        std::string displayName = templateId;
        std::string previewSprite;
        if (schemaService_) {
            if (const auto* descriptor = schemaService_->GetTemplate(templateId)) {
                displayName = descriptor->displayName.empty() ? templateId : descriptor->displayName;
                previewSprite = descriptor->spriteName;
            }
        }
        DrawText(displayName, tile.x + kTemplateTilePadding, tile.y + 10, kTextColor);

        if (assetCache_) {
            const auto* spriteInfo = assetCache_->GetSpriteInfo(previewSprite.empty() ? templateId : previewSprite);
            if (spriteInfo && !spriteInfo->frames.empty()) {
                SDL_Texture* texture = assetCache_->GetTexture(spriteInfo->frames[0].textureName);
                if (texture) {
                    SDL_Rect src{spriteInfo->frames[0].x, spriteInfo->frames[0].y, spriteInfo->frames[0].w, spriteInfo->frames[0].h};
                    int previewSize = kTemplateTileHeight - 20;
                    SDL_Rect dst{tile.x + tile.w - previewSize - kTemplateTilePadding,
                                 tile.y + kTemplateTilePadding,
                                 previewSize,
                                 previewSize};
                    SDL_RenderCopy(renderer_, texture, &src, &dst);
                }
            }
        }

        y += kTemplateTileHeight + kTemplateTilePadding;
        if (y + kTemplateTileHeight > height) {
            break;
        }
    }
}

bool ToolbarPanel::HandleEvent(const SDL_Event& event, int width, int height) {
    if (event.type == SDL_MOUSEBUTTONDOWN) {
        return HandleMouseButtonEvent(event, width, height);
    }
    if (event.type == SDL_MOUSEWHEEL) {
        return true;
    }
    return false;
}

bool ToolbarPanel::HandleMouseButtonEvent(const SDL_Event& event, int width, int height) {
    if (event.type != SDL_MOUSEBUTTONDOWN) {
        return false;
    }
    const int x = event.button.x;
    const int y = event.button.y;
    if (x < 0 || x > width || y < 0 || y > height) {
        return false;
    }

    // Buttons
    struct ButtonArea {
        SDL_Rect rect;
        std::function<void()> callback;
    };

    std::vector<ButtonArea> areas;
    const auto buttons = BuildButtonDefs();
    const int buttonBottom = LayoutButtons(
        width,
        buttons,
        [&](const ButtonDef& button, const SDL_Rect& rect) {
            areas.push_back({rect, button.callback});
        });

    SDL_Point point{x, y};
    for (const auto& area : areas) {
        if (SDL_PointInRect(&point, &area.rect)) {
            if (area.callback) {
                area.callback();
            }
            return true;
        }
    }

    // Template tiles
    const int tileBaseline = buttonBottom > 0 ? buttonBottom : (kPanelPadding + kButtonHeight);
    int tileY = tileBaseline + kPanelPadding;
    const int tileWidth = width - kPanelPadding * 2;
    for (const auto& templateId : templateIds_) {
        SDL_Rect tile{kPanelPadding, tileY, tileWidth, kTemplateTileHeight};
        if (SDL_PointInRect(&point, &tile)) {
            selectedTemplate_ = templateId;
            if (event.button.button == SDL_BUTTON_RIGHT) {
                if (callbacks_.onEditTemplate) {
                    callbacks_.onEditTemplate(templateId);
                }
                return true;
            } else {
                if (callbacks_.onTemplateSelected) {
                    callbacks_.onTemplateSelected(templateId);
                }
                if (callbacks_.onBeginDrag && event.button.button == SDL_BUTTON_LEFT) {
                    callbacks_.onBeginDrag(templateId, event.button.x, event.button.y);
                }
                return true;
            }
        }
        tileY += kTemplateTileHeight + kTemplateTilePadding;
    }

    return true;
}

std::vector<ToolbarPanel::ButtonDef> ToolbarPanel::BuildButtonDefs() const {
    return {
        {"New", callbacks_.onNew},
        {"Load", callbacks_.onLoad},
        {"Save", callbacks_.onSave},
        {"Apply", [this]() {
             if (callbacks_.onApplyTemplate && !selectedTemplate_.empty()) {
                 callbacks_.onApplyTemplate(selectedTemplate_);
             }
         }},
    };
}

int ToolbarPanel::LayoutButtons(
    int width,
    const std::vector<ButtonDef>& buttons,
    const std::function<void(const ButtonDef&, const SDL_Rect&)>& visitor) const {
    if (buttons.empty()) {
        return kPanelPadding;
    }

    const int availableWidth = std::max(1, width - kPanelPadding * 2);
    const int cellWidth = kButtonWidth + kButtonSpacing;
    int columns = std::max(1, availableWidth / cellWidth);
    int rows = static_cast<int>((buttons.size() + columns - 1) / columns);

    for (size_t index = 0; index < buttons.size(); ++index) {
        const int row = static_cast<int>(index) / columns;
        const int col = static_cast<int>(index) % columns;
        SDL_Rect rect{
            kPanelPadding + col * (kButtonWidth + kButtonSpacing),
            kPanelPadding + row * (kButtonHeight + kButtonSpacing),
            kButtonWidth,
            kButtonHeight};
        if (visitor) {
            visitor(buttons[index], rect);
        }
    }

    return kPanelPadding + rows * kButtonHeight + (rows - 1) * kButtonSpacing;
}

void ToolbarPanel::DrawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!font_ || !renderer_) {
        return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surface) {
        return;
    }
    const int width = surface->w;
    const int height = surface->h;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        return;
    }
    SDL_Rect dst{x, y, width, height};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

}  // namespace level_editor

