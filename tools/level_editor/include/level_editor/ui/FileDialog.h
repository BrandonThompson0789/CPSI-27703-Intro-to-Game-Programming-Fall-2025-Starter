#pragma once

#include <functional>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_ttf.h>

namespace level_editor {

class FileDialog {
public:
    enum class Mode { None, Load };

    FileDialog();
    void Initialize(SDL_Renderer* renderer, TTF_Font* font);

    void OpenLoad(const std::vector<std::string>& files,
                  std::function<void(const std::string&)> onConfirm);
    void Close();
    bool IsOpen() const { return mode_ != Mode::None; }

    void Render(int screenWidth, int screenHeight);
    bool HandleEvent(const SDL_Event& event);

private:
    bool HandleMouseButtonEvent(const SDL_Event& event);
    void DrawText(const std::string& text, int x, int y, SDL_Color color);

    SDL_Renderer* renderer_{nullptr};
    TTF_Font* font_{nullptr};
    Mode mode_{Mode::None};
    std::vector<std::string> entries_;
    int hoveredIndex_{-1};
    std::function<void(const std::string&)> onConfirm_;
    SDL_Rect dialogRect_{};
};

}  // namespace level_editor

