#include "level_editor/ui/FileDialog.h"

#include <algorithm>

namespace level_editor {

namespace {
constexpr SDL_Color kDialogBg{20, 22, 26, 240};
constexpr SDL_Color kListBg{36, 38, 44, 255};
constexpr SDL_Color kListHover{70, 120, 180, 255};
constexpr SDL_Color kTextColor{230, 230, 235, 255};
constexpr int kDialogWidth = 420;
constexpr int kDialogHeight = 480;
constexpr int kPadding = 16;
constexpr int kRowHeight = 32;
}  // namespace

FileDialog::FileDialog() = default;

void FileDialog::Initialize(SDL_Renderer* renderer, TTF_Font* font) {
    renderer_ = renderer;
    font_ = font;
}

void FileDialog::OpenLoad(const std::vector<std::string>& files,
                          std::function<void(const std::string&)> onConfirm) {
    entries_ = files;
    std::sort(entries_.begin(), entries_.end());
    mode_ = Mode::Load;
    onConfirm_ = std::move(onConfirm);
}

void FileDialog::Close() {
    mode_ = Mode::None;
    entries_.clear();
    hoveredIndex_ = -1;
    onConfirm_ = nullptr;
}

void FileDialog::Render(int screenWidth, int screenHeight) {
    if (mode_ == Mode::None || !renderer_) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 160);
    SDL_Rect full{0, 0, screenWidth, screenHeight};
    SDL_RenderFillRect(renderer_, &full);

    dialogRect_ = {screenWidth / 2 - kDialogWidth / 2,
                   screenHeight / 2 - kDialogHeight / 2,
                   kDialogWidth,
                   kDialogHeight};
    SDL_SetRenderDrawColor(renderer_, kDialogBg.r, kDialogBg.g, kDialogBg.b, kDialogBg.a);
    SDL_RenderFillRect(renderer_, &dialogRect_);

    SDL_Rect listRect{dialogRect_.x + kPadding,
                      dialogRect_.y + kPadding * 2,
                      dialogRect_.w - kPadding * 2,
                      dialogRect_.h - kPadding * 3};
    SDL_SetRenderDrawColor(renderer_, kListBg.r, kListBg.g, kListBg.b, kListBg.a);
    SDL_RenderFillRect(renderer_, &listRect);

    DrawText(mode_ == Mode::Load ? "Select Level to Load" : "Select Level",
             dialogRect_.x + kPadding,
             dialogRect_.y + kPadding / 2,
             kTextColor);

    int rowY = listRect.y;
    for (size_t i = 0; i < entries_.size(); ++i) {
        SDL_Rect row{listRect.x, rowY, listRect.w, kRowHeight};
        if (static_cast<int>(i) == hoveredIndex_) {
            SDL_SetRenderDrawColor(renderer_, kListHover.r, kListHover.g, kListHover.b, 200);
            SDL_RenderFillRect(renderer_, &row);
        }
        DrawText(entries_[i], row.x + 8, row.y + 6, kTextColor);
        rowY += kRowHeight;
        if (rowY + kRowHeight > listRect.y + listRect.h) {
            break;
        }
    }
}

bool FileDialog::HandleEvent(const SDL_Event& event) {
    if (mode_ == Mode::None) {
        return false;
    }
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            const int x = event.motion.x;
            const int y = event.motion.y;
            SDL_Point point{x, y};
            if (!SDL_PointInRect(&point, &dialogRect_)) {
                hoveredIndex_ = -1;
                return true;
            }
            SDL_Rect listRect{dialogRect_.x + kPadding,
                              dialogRect_.y + kPadding * 2,
                              dialogRect_.w - kPadding * 2,
                              dialogRect_.h - kPadding * 3};
            if (!SDL_PointInRect(&point, &listRect)) {
                hoveredIndex_ = -1;
                return true;
            }
            const int relativeY = y - listRect.y;
            hoveredIndex_ = relativeY / kRowHeight;
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
            return HandleMouseButtonEvent(event);
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                Close();
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

bool FileDialog::HandleMouseButtonEvent(const SDL_Event& event) {
    const int x = event.button.x;
    const int y = event.button.y;
    SDL_Point point{x, y};
    if (!SDL_PointInRect(&point, &dialogRect_)) {
        Close();
        return true;
    }

    SDL_Rect listRect{dialogRect_.x + kPadding,
                      dialogRect_.y + kPadding * 2,
                      dialogRect_.w - kPadding * 2,
                      dialogRect_.h - kPadding * 3};
    if (!SDL_PointInRect(&point, &listRect)) {
        return true;
    }

    const int index = (y - listRect.y) / kRowHeight;
    if (index >= 0 && index < static_cast<int>(entries_.size())) {
        if (onConfirm_) {
            onConfirm_(entries_[static_cast<size_t>(index)]);
        }
        Close();
    }
    return true;
}

void FileDialog::DrawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!renderer_ || !font_) {
        return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
    if (!surface) {
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) {
        return;
    }
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

}  // namespace level_editor

