#pragma once

#include <functional>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#include <nlohmann/json.hpp>

namespace level_editor {

struct TemplateEditResult {
    std::string templateId;
    nlohmann::json templateJson;
    bool applyToExisting{true};
};

class TemplateEditorModal {
public:
    TemplateEditorModal();

    void Initialize(SDL_Renderer* renderer, TTF_Font* font);
    void Open(const std::string& templateId,
              const nlohmann::json& templateJson,
              std::function<void(const TemplateEditResult&)> onSubmit);
    void Close();

    bool IsOpen() const { return open_; }
    bool HandleEvent(const SDL_Event& event);
    void Render(int windowWidth, int windowHeight);

private:
    struct TextField {
        SDL_Rect rect{0, 0, 0, 0};
        std::string value;
        bool focused{false};
        size_t cursor{0};

        void SetValue(const std::string& text);
        bool HandleMouseDown(int x, int y);
        bool HandleTextInput(const SDL_TextInputEvent& textEvent);
        bool HandleKeyDown(const SDL_KeyboardEvent& keyEvent);
        void Defocus();
    };

    void BeginTextInput();
    void EndTextInput();
    void ResetFields();
    void LoadFromTemplate();
    bool CommitEdits();
    void DrawText(const std::string& text, int x, int y, SDL_Color color);
    void DrawField(const std::string& label, TextField& field, int x, int y, int width);
    void DrawSectionHeader(const std::string& label, int x, int y, bool expanded, SDL_Rect* headerRect);
    void ComputeLayout(int windowWidth, int windowHeight);
    void RenderCheckbox(int x, int y);
    void AdjustScroll(int delta);
    void UpdateScrollBounds(int totalContentHeight);
    bool ToggleSectionAt(int x, int y);
    nlohmann::json* FindComponent(nlohmann::json& target, const std::string& type);
    bool AssignNumberField(nlohmann::json& target, const std::string& key, const TextField& field, std::string_view label);
    bool AssignIntegerField(nlohmann::json& target, const std::string& key, const TextField& field, std::string_view label, int minValue, int maxValue);
    std::string FormatNumber(double value) const;
    void ClearFieldFocus();

    SDL_Renderer* renderer_{nullptr};
    TTF_Font* font_{nullptr};

    bool open_{false};
    bool textInputActive_{false};
    bool applyToExisting_{true};
    std::function<void(const TemplateEditResult&)> submitCallback_;
    TemplateEditResult pendingResult_;
    nlohmann::json templateData_;
    std::string templateId_;
    std::string errorMessage_;

    SDL_Rect panelRect_{0, 0, 0, 0};
    SDL_Rect saveButtonRect_{0, 0, 0, 0};
    SDL_Rect cancelButtonRect_{0, 0, 0, 0};
    SDL_Rect checkboxRect_{0, 0, 0, 0};
    SDL_Rect contentAreaRect_{0, 0, 0, 0};
    SDL_Rect bodyHeaderRect_{0, 0, 0, 0};
    SDL_Rect spriteHeaderRect_{0, 0, 0, 0};

    TextField displayNameField_;
    TextField spriteNameField_;
    TextField spriteRenderWidthField_;
    TextField spriteRenderHeightField_;
    TextField spriteOffsetXField_;
    TextField spriteOffsetYField_;
    TextField spriteAngleField_;
    TextField spriteColorRField_;
    TextField spriteColorGField_;
    TextField spriteColorBField_;
    TextField spriteAlphaField_;

    TextField bodyPosXField_;
    TextField bodyPosYField_;
    TextField bodyAngleField_;
    TextField bodyWidthField_;
    TextField bodyHeightField_;
    TextField bodyRadiusField_;
    TextField fixtureDensityField_;
    TextField fixtureFrictionField_;
    TextField fixtureRestitutionField_;

    bool bodyExpanded_{true};
    bool spriteExpanded_{true};
    int contentScroll_{0};
    int maxContentScroll_{0};
};

}  // namespace level_editor


