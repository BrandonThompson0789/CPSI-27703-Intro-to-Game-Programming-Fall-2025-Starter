#include "level_editor/ui/TemplateEditorModal.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace level_editor {

namespace {
constexpr SDL_Color kPanelColor{32, 34, 40, 255};
constexpr SDL_Color kPanelBorderColor{90, 95, 105, 255};
constexpr SDL_Color kOverlayColor{0, 0, 0, 120};
constexpr SDL_Color kTextColor{225, 225, 230, 255};
constexpr SDL_Color kHighlightColor{255, 210, 64, 255};
constexpr SDL_Color kErrorColor{220, 96, 86, 255};
constexpr int kScrollStep = 40;

bool PointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void TrimTrailingZeros(std::string& value) {
    if (auto dot = value.find('.'); dot != std::string::npos) {
        while (!value.empty() && value.back() == '0') {
            value.pop_back();
        }
        if (!value.empty() && value.back() == '.') {
            value.pop_back();
        }
    }
}

}  // namespace

void TemplateEditorModal::TextField::SetValue(const std::string& text) {
    value = text;
    cursor = value.size();
}

bool TemplateEditorModal::TextField::HandleMouseDown(int x, int y) {
    if (!PointInRect(x, y, rect)) {
        return false;
    }
    focused = true;
    cursor = value.size();
    return true;
}

bool TemplateEditorModal::TextField::HandleTextInput(const SDL_TextInputEvent& textEvent) {
    if (!focused) {
        return false;
    }
    value.insert(cursor, textEvent.text);
    cursor += SDL_strlen(textEvent.text);
    return true;
}

bool TemplateEditorModal::TextField::HandleKeyDown(const SDL_KeyboardEvent& keyEvent) {
    if (!focused) {
        return false;
    }
    switch (keyEvent.keysym.sym) {
        case SDLK_BACKSPACE:
            if (cursor > 0 && !value.empty()) {
                value.erase(cursor - 1, 1);
                --cursor;
            }
            return true;
        case SDLK_DELETE:
            if (cursor < value.size()) {
                value.erase(cursor, 1);
            }
            return true;
        case SDLK_LEFT:
            if (cursor > 0) {
                --cursor;
            }
            return true;
        case SDLK_RIGHT:
            if (cursor < value.size()) {
                ++cursor;
            }
            return true;
        case SDLK_HOME:
            cursor = 0;
            return true;
        case SDLK_END:
            cursor = value.size();
            return true;
        default:
            break;
    }
    return false;
}

void TemplateEditorModal::TextField::Defocus() {
    focused = false;
}

TemplateEditorModal::TemplateEditorModal() = default;

void TemplateEditorModal::Initialize(SDL_Renderer* renderer, TTF_Font* font) {
    renderer_ = renderer;
    font_ = font;
}

void TemplateEditorModal::BeginTextInput() {
    if (!textInputActive_) {
        SDL_StartTextInput();
        textInputActive_ = true;
    }
}

void TemplateEditorModal::EndTextInput() {
    if (textInputActive_) {
        SDL_StopTextInput();
        textInputActive_ = false;
    }
}

void TemplateEditorModal::Open(const std::string& templateId,
                               const nlohmann::json& templateJson,
                               std::function<void(const TemplateEditResult&)> onSubmit) {
    templateId_ = templateId;
    templateData_ = templateJson;
    submitCallback_ = std::move(onSubmit);
    applyToExisting_ = true;
    errorMessage_.clear();
    contentScroll_ = 0;
    maxContentScroll_ = 0;
    bodyExpanded_ = true;
    spriteExpanded_ = true;
    ResetFields();
    LoadFromTemplate();
    open_ = true;
    BeginTextInput();
}

void TemplateEditorModal::Close() {
    open_ = false;
    submitCallback_ = nullptr;
    displayNameField_.Defocus();
    spriteNameField_.Defocus();
    EndTextInput();
}

bool TemplateEditorModal::HandleEvent(const SDL_Event& event) {
    if (!open_) {
        return false;
    }

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN: {
            const int x = event.button.x;
            const int y = event.button.y;
            bool consumed = false;
            if (!PointInRect(x, y, panelRect_)) {
                Close();
                return true;
            }
            ClearFieldFocus();
            if (ToggleSectionAt(x, y)) {
                return true;
            }
            consumed |= displayNameField_.HandleMouseDown(x, y);
            consumed |= spriteNameField_.HandleMouseDown(x, y);
            consumed |= bodyPosXField_.HandleMouseDown(x, y);
            consumed |= bodyPosYField_.HandleMouseDown(x, y);
            consumed |= bodyAngleField_.HandleMouseDown(x, y);
            consumed |= bodyWidthField_.HandleMouseDown(x, y);
            consumed |= bodyHeightField_.HandleMouseDown(x, y);
            consumed |= bodyRadiusField_.HandleMouseDown(x, y);
            consumed |= spriteRenderWidthField_.HandleMouseDown(x, y);
            consumed |= spriteRenderHeightField_.HandleMouseDown(x, y);
            consumed |= spriteColorRField_.HandleMouseDown(x, y);
            consumed |= spriteColorGField_.HandleMouseDown(x, y);
            consumed |= spriteColorBField_.HandleMouseDown(x, y);
            consumed |= spriteAlphaField_.HandleMouseDown(x, y);

            if (PointInRect(x, y, checkboxRect_)) {
                applyToExisting_ = !applyToExisting_;
                consumed = true;
            }
            if (PointInRect(x, y, saveButtonRect_)) {
                consumed = true;
                if (CommitEdits()) {
                    Close();
                }
            } else if (PointInRect(x, y, cancelButtonRect_)) {
                Close();
                consumed = true;
            }
            return consumed;
        }
        case SDL_MOUSEWHEEL:
            AdjustScroll(-event.wheel.y * kScrollStep);
            return true;
        case SDL_TEXTINPUT:
            if (displayNameField_.HandleTextInput(event.text) || spriteNameField_.HandleTextInput(event.text) ||
                bodyPosXField_.HandleTextInput(event.text) || bodyPosYField_.HandleTextInput(event.text) ||
                bodyAngleField_.HandleTextInput(event.text) || bodyWidthField_.HandleTextInput(event.text) ||
                bodyHeightField_.HandleTextInput(event.text) || bodyRadiusField_.HandleTextInput(event.text) ||
                spriteRenderWidthField_.HandleTextInput(event.text) || spriteRenderHeightField_.HandleTextInput(event.text) ||
                spriteOffsetXField_.HandleTextInput(event.text) || spriteOffsetYField_.HandleTextInput(event.text) ||
                spriteAngleField_.HandleTextInput(event.text) || spriteColorRField_.HandleTextInput(event.text) ||
                spriteColorGField_.HandleTextInput(event.text) || spriteColorBField_.HandleTextInput(event.text) ||
                spriteAlphaField_.HandleTextInput(event.text) || fixtureDensityField_.HandleTextInput(event.text) ||
                fixtureFrictionField_.HandleTextInput(event.text) || fixtureRestitutionField_.HandleTextInput(event.text)) {
                return true;
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                Close();
                return true;
            }
            if (displayNameField_.HandleKeyDown(event.key) || spriteNameField_.HandleKeyDown(event.key) ||
                bodyPosXField_.HandleKeyDown(event.key) || bodyPosYField_.HandleKeyDown(event.key) ||
                bodyAngleField_.HandleKeyDown(event.key) || bodyWidthField_.HandleKeyDown(event.key) ||
                bodyHeightField_.HandleKeyDown(event.key) || bodyRadiusField_.HandleKeyDown(event.key) ||
                spriteRenderWidthField_.HandleKeyDown(event.key) || spriteRenderHeightField_.HandleKeyDown(event.key) ||
                spriteOffsetXField_.HandleKeyDown(event.key) || spriteOffsetYField_.HandleKeyDown(event.key) ||
                spriteAngleField_.HandleKeyDown(event.key) || spriteColorRField_.HandleKeyDown(event.key) ||
                spriteColorGField_.HandleKeyDown(event.key) || spriteColorBField_.HandleKeyDown(event.key) ||
                spriteAlphaField_.HandleKeyDown(event.key) || fixtureDensityField_.HandleKeyDown(event.key) ||
                fixtureFrictionField_.HandleKeyDown(event.key) || fixtureRestitutionField_.HandleKeyDown(event.key)) {
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

void TemplateEditorModal::ClearFieldFocus() {
    displayNameField_.Defocus();
    spriteNameField_.Defocus();
    bodyPosXField_.Defocus();
    bodyPosYField_.Defocus();
    bodyAngleField_.Defocus();
    bodyWidthField_.Defocus();
    bodyHeightField_.Defocus();
    bodyRadiusField_.Defocus();
    spriteRenderWidthField_.Defocus();
    spriteRenderHeightField_.Defocus();
    spriteOffsetXField_.Defocus();
    spriteOffsetYField_.Defocus();
    spriteAngleField_.Defocus();
    spriteColorRField_.Defocus();
    spriteColorGField_.Defocus();
    spriteColorBField_.Defocus();
    spriteAlphaField_.Defocus();
    fixtureDensityField_.Defocus();
    fixtureFrictionField_.Defocus();
    fixtureRestitutionField_.Defocus();
}

void TemplateEditorModal::Render(int windowWidth, int windowHeight) {
    if (!open_) {
        return;
    }

    ComputeLayout(windowWidth, windowHeight);

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, kOverlayColor.r, kOverlayColor.g, kOverlayColor.b, kOverlayColor.a);
    SDL_Rect overlay{0, 0, windowWidth, windowHeight};
    SDL_RenderFillRect(renderer_, &overlay);

    SDL_SetRenderDrawColor(renderer_, kPanelColor.r, kPanelColor.g, kPanelColor.b, kPanelColor.a);
    SDL_RenderFillRect(renderer_, &panelRect_);
    SDL_SetRenderDrawColor(renderer_, kPanelBorderColor.r, kPanelBorderColor.g, kPanelBorderColor.b, kPanelBorderColor.a);
    SDL_RenderDrawRect(renderer_, &panelRect_);

    DrawText("Template Editor", panelRect_.x + 20, panelRect_.y + 20, kHighlightColor);
    DrawText("Editing: " + templateId_, panelRect_.x + 20, panelRect_.y + 45, kTextColor);

    const int contentMarginX = 20;
    const int contentLeft = panelRect_.x + contentMarginX;
    const int contentRight = panelRect_.x + panelRect_.w - contentMarginX;
    contentAreaRect_ = {panelRect_.x + 10,
                        panelRect_.y + 70,
                        panelRect_.w - 20,
                        std::max(40, saveButtonRect_.y - (panelRect_.y + 150))};
    const int fieldWidth = contentRight - contentLeft;
    const int halfWidth = fieldWidth / 2 - 10;
    const int quarterWidth = fieldWidth / 4 - 10;
    const int thirdWidth = fieldWidth / 3 - 14;
    const int columnSpacing = 10;

    int logicalY = panelRect_.y + 80;
    const int contentStart = logicalY;
    SDL_RenderSetClipRect(renderer_, &contentAreaRect_);

    auto nextY = [&](int amount) {
        logicalY += amount;
    };

    DrawField("Display Name", displayNameField_, contentLeft, logicalY - contentScroll_, fieldWidth);
    nextY(50);
    DrawField("Sprite Name", spriteNameField_, contentLeft, logicalY - contentScroll_, fieldWidth);
    nextY(60);

    const int headerHeight = 34;
    const int bodyHeaderY = logicalY - contentScroll_ - 12;
    DrawSectionHeader("Body Component", contentLeft, bodyHeaderY, bodyExpanded_, &bodyHeaderRect_);
    nextY(headerHeight);
    if (bodyExpanded_) {
        DrawField("Position X", bodyPosXField_, contentLeft, logicalY - contentScroll_, halfWidth);
        DrawField("Position Y", bodyPosYField_, contentLeft + halfWidth + columnSpacing, logicalY - contentScroll_, halfWidth);
        nextY(50);
        DrawField("Angle", bodyAngleField_, contentLeft, logicalY - contentScroll_, halfWidth);
        DrawField("Width", bodyWidthField_, contentLeft + halfWidth + columnSpacing, logicalY - contentScroll_, halfWidth);
        nextY(50);
        DrawField("Height", bodyHeightField_, contentLeft, logicalY - contentScroll_, halfWidth);
        DrawField("Radius", bodyRadiusField_, contentLeft + halfWidth + columnSpacing, logicalY - contentScroll_, halfWidth);
        nextY(50);
        DrawField("Density", fixtureDensityField_, contentLeft, logicalY - contentScroll_, thirdWidth);
        DrawField("Friction",
                  fixtureFrictionField_,
                  contentLeft + thirdWidth + columnSpacing,
                  logicalY - contentScroll_,
                  thirdWidth);
        DrawField("Restitution",
                  fixtureRestitutionField_,
                  contentLeft + 2 * (thirdWidth + columnSpacing),
                  logicalY - contentScroll_,
                  thirdWidth);
        nextY(60);
    }

    const int spriteHeaderY = logicalY - contentScroll_ - 12;
    DrawSectionHeader("Sprite Component", contentLeft, spriteHeaderY, spriteExpanded_, &spriteHeaderRect_);
    nextY(headerHeight);
    if (spriteExpanded_) {
        DrawField("Render Width", spriteRenderWidthField_, contentLeft, logicalY - contentScroll_, halfWidth);
        DrawField("Render Height",
                  spriteRenderHeightField_,
                  contentLeft + halfWidth + columnSpacing,
                  logicalY - contentScroll_,
                  halfWidth);
        nextY(50);
        DrawField("Sprite Offset X", spriteOffsetXField_, contentLeft, logicalY - contentScroll_, thirdWidth);
        DrawField("Sprite Offset Y",
                  spriteOffsetYField_,
                  contentLeft + thirdWidth + columnSpacing,
                  logicalY - contentScroll_,
                  thirdWidth);
        DrawField("Sprite Angle",
                  spriteAngleField_,
                  contentLeft + 2 * (thirdWidth + columnSpacing),
                  logicalY - contentScroll_,
                  thirdWidth);
        nextY(50);
        DrawField("Color R", spriteColorRField_, contentLeft, logicalY - contentScroll_, quarterWidth);
        DrawField("Color G",
                  spriteColorGField_,
                  contentLeft + quarterWidth + columnSpacing,
                  logicalY - contentScroll_,
                  quarterWidth);
        DrawField("Color B",
                  spriteColorBField_,
                  contentLeft + 2 * (quarterWidth + columnSpacing),
                  logicalY - contentScroll_,
                  quarterWidth);
        DrawField("Alpha",
                  spriteAlphaField_,
                  contentLeft + 3 * (quarterWidth + columnSpacing),
                  logicalY - contentScroll_,
                  quarterWidth);
        nextY(60);
    }

    SDL_RenderSetClipRect(renderer_, nullptr);
    UpdateScrollBounds(logicalY - contentStart);

    const int checkboxY = saveButtonRect_.y - 70;
    RenderCheckbox(contentLeft, checkboxY);

    SDL_SetRenderDrawColor(renderer_, 70, 140, 90, 255);
    SDL_RenderFillRect(renderer_, &saveButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 30, 50, 35, 255);
    SDL_RenderDrawRect(renderer_, &saveButtonRect_);
    DrawText("Save", saveButtonRect_.x + 30, saveButtonRect_.y + 10, kTextColor);

    SDL_SetRenderDrawColor(renderer_, 120, 60, 60, 255);
    SDL_RenderFillRect(renderer_, &cancelButtonRect_);
    SDL_SetRenderDrawColor(renderer_, 50, 30, 30, 255);
    SDL_RenderDrawRect(renderer_, &cancelButtonRect_);
    DrawText("Cancel", cancelButtonRect_.x + 20, cancelButtonRect_.y + 10, kTextColor);

    if (!errorMessage_.empty()) {
        DrawText(errorMessage_, panelRect_.x + 20, saveButtonRect_.y - 30, kErrorColor);
    }
}

void TemplateEditorModal::ResetFields() {
    displayNameField_.SetValue("");
    spriteNameField_.SetValue("");
    bodyPosXField_.SetValue("");
    bodyPosYField_.SetValue("");
    bodyAngleField_.SetValue("");
    bodyWidthField_.SetValue("");
    bodyHeightField_.SetValue("");
    bodyRadiusField_.SetValue("");
    spriteRenderWidthField_.SetValue("");
    spriteRenderHeightField_.SetValue("");
    spriteOffsetXField_.SetValue("");
    spriteOffsetYField_.SetValue("");
    spriteAngleField_.SetValue("");
    spriteColorRField_.SetValue("");
    spriteColorGField_.SetValue("");
    spriteColorBField_.SetValue("");
    spriteAlphaField_.SetValue("");
    fixtureDensityField_.SetValue("");
    fixtureFrictionField_.SetValue("");
    fixtureRestitutionField_.SetValue("");
}

void TemplateEditorModal::LoadFromTemplate() {
    displayNameField_.SetValue(templateData_.value("name", templateId_));

    if (auto* sprite = FindComponent(templateData_, "SpriteComponent"); sprite) {
        spriteNameField_.SetValue(sprite->value("spriteName", ""));
        spriteRenderWidthField_.SetValue(sprite->contains("renderWidth") ? FormatNumber((*sprite)["renderWidth"].get<double>()) : "");
        spriteRenderHeightField_.SetValue(sprite->contains("renderHeight") ? FormatNumber((*sprite)["renderHeight"].get<double>()) : "");
        spriteOffsetXField_.SetValue(sprite->contains("posX") ? FormatNumber((*sprite)["posX"].get<double>()) : "");
        spriteOffsetYField_.SetValue(sprite->contains("posY") ? FormatNumber((*sprite)["posY"].get<double>()) : "");
        spriteAngleField_.SetValue(sprite->contains("angle") ? FormatNumber((*sprite)["angle"].get<double>()) : "");
        spriteColorRField_.SetValue(sprite->contains("colorR") ? std::to_string((*sprite)["colorR"].get<int>()) : "");
        spriteColorGField_.SetValue(sprite->contains("colorG") ? std::to_string((*sprite)["colorG"].get<int>()) : "");
        spriteColorBField_.SetValue(sprite->contains("colorB") ? std::to_string((*sprite)["colorB"].get<int>()) : "");
        spriteAlphaField_.SetValue(sprite->contains("alpha") ? std::to_string((*sprite)["alpha"].get<int>()) : "");
    }

    if (auto* body = FindComponent(templateData_, "BodyComponent"); body) {
        bodyPosXField_.SetValue(body->contains("posX") ? FormatNumber((*body)["posX"].get<double>()) : "");
        bodyPosYField_.SetValue(body->contains("posY") ? FormatNumber((*body)["posY"].get<double>()) : "");
        bodyAngleField_.SetValue(body->contains("angle") ? FormatNumber((*body)["angle"].get<double>()) : "");
        if (!body->contains("fixture") || !(*body)["fixture"].is_object()) {
            (*body)["fixture"] = nlohmann::json::object();
        }
        auto& fixture = (*body)["fixture"];
        bodyWidthField_.SetValue(fixture.contains("width") ? FormatNumber(fixture["width"].get<double>()) : "");
        bodyHeightField_.SetValue(fixture.contains("height") ? FormatNumber(fixture["height"].get<double>()) : "");
        bodyRadiusField_.SetValue(fixture.contains("radius") ? FormatNumber(fixture["radius"].get<double>()) : "");
        fixtureDensityField_.SetValue(fixture.contains("density") ? FormatNumber(fixture["density"].get<double>()) : "");
        fixtureFrictionField_.SetValue(fixture.contains("friction") ? FormatNumber(fixture["friction"].get<double>()) : "");
        fixtureRestitutionField_.SetValue(fixture.contains("restitution") ? FormatNumber(fixture["restitution"].get<double>()) : "");
    }
}

std::string TemplateEditorModal::FormatNumber(double value) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << value;
    auto result = oss.str();
    TrimTrailingZeros(result);
    return result;
}

nlohmann::json* TemplateEditorModal::FindComponent(nlohmann::json& target, const std::string& type) {
    auto compsIt = target.find("components");
    if (compsIt == target.end() || !compsIt->is_array()) {
        return nullptr;
    }
    for (auto& component : *compsIt) {
        if (component.contains("type") && component["type"].is_string() && component["type"].get<std::string>() == type) {
            return &component;
        }
    }
    return nullptr;
}

bool TemplateEditorModal::AssignNumberField(nlohmann::json& target, const std::string& key, const TextField& field, std::string_view label) {
    if (field.value.empty()) {
        target.erase(key);
        return true;
    }
    try {
        double value = std::stod(field.value);
        target[key] = value;
    } catch (const std::exception&) {
        errorMessage_ = "Invalid number for " + std::string(label);
        return false;
    }
    return true;
}

bool TemplateEditorModal::AssignIntegerField(nlohmann::json& target,
                                             const std::string& key,
                                             const TextField& field,
                                             std::string_view label,
                                             int minValue,
                                             int maxValue) {
    if (field.value.empty()) {
        target.erase(key);
        return true;
    }
    try {
        int value = std::stoi(field.value);
        value = std::clamp(value, minValue, maxValue);
        target[key] = value;
    } catch (const std::exception&) {
        errorMessage_ = "Invalid number for " + std::string(label);
        return false;
    }
    return true;
}

bool TemplateEditorModal::CommitEdits() {
    pendingResult_.templateId = templateId_;
    pendingResult_.templateJson = templateData_;
    pendingResult_.applyToExisting = applyToExisting_;
    errorMessage_.clear();

    auto& json = pendingResult_.templateJson;
    if (!displayNameField_.value.empty()) {
        json["name"] = displayNameField_.value;
    } else {
        json["name"] = templateId_;
    }

    nlohmann::json* sprite = FindComponent(json, "SpriteComponent");
    if (sprite) {
        if (!spriteNameField_.value.empty()) {
            (*sprite)["spriteName"] = spriteNameField_.value;
        } else {
            sprite->erase("spriteName");
        }
        if (!AssignNumberField(*sprite, "renderWidth", spriteRenderWidthField_, "Render Width") ||
            !AssignNumberField(*sprite, "renderHeight", spriteRenderHeightField_, "Render Height") ||
            !AssignNumberField(*sprite, "posX", spriteOffsetXField_, "Sprite Offset X") ||
            !AssignNumberField(*sprite, "posY", spriteOffsetYField_, "Sprite Offset Y") ||
            !AssignNumberField(*sprite, "angle", spriteAngleField_, "Sprite Angle") ||
            !AssignIntegerField(*sprite, "colorR", spriteColorRField_, "Color R", 0, 255) ||
            !AssignIntegerField(*sprite, "colorG", spriteColorGField_, "Color G", 0, 255) ||
            !AssignIntegerField(*sprite, "colorB", spriteColorBField_, "Color B", 0, 255) ||
            !AssignIntegerField(*sprite, "alpha", spriteAlphaField_, "Alpha", 0, 255)) {
            return false;
        }
    }

    nlohmann::json* body = FindComponent(json, "BodyComponent");
    if (body) {
        if (!AssignNumberField(*body, "posX", bodyPosXField_, "Body Pos X") ||
            !AssignNumberField(*body, "posY", bodyPosYField_, "Body Pos Y") ||
            !AssignNumberField(*body, "angle", bodyAngleField_, "Body Angle")) {
            return false;
        }
        if (!body->contains("fixture") || !(*body)["fixture"].is_object()) {
            (*body)["fixture"] = nlohmann::json::object();
        }
        auto& fixture = (*body)["fixture"];
        if (!AssignNumberField(fixture, "width", bodyWidthField_, "Body Width") ||
            !AssignNumberField(fixture, "height", bodyHeightField_, "Body Height") ||
            !AssignNumberField(fixture, "radius", bodyRadiusField_, "Body Radius") ||
            !AssignNumberField(fixture, "density", fixtureDensityField_, "Body Density") ||
            !AssignNumberField(fixture, "friction", fixtureFrictionField_, "Body Friction") ||
            !AssignNumberField(fixture, "restitution", fixtureRestitutionField_, "Body Restitution")) {
            return false;
        }
    }

    if (submitCallback_) {
        submitCallback_(pendingResult_);
    }
    return true;
}

void TemplateEditorModal::DrawText(const std::string& text, int x, int y, SDL_Color color) {
    if (!font_ || text.empty()) {
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

void TemplateEditorModal::DrawField(const std::string& label, TextField& field, int x, int y, int width) {
    DrawText(label, x, y - 18, kTextColor);
    field.rect = {x, y, width, 32};
    SDL_SetRenderDrawColor(renderer_, 45, 48, 55, 255);
    SDL_RenderFillRect(renderer_, &field.rect);
    SDL_SetRenderDrawColor(renderer_, field.focused ? kHighlightColor.r : 80,
                           field.focused ? kHighlightColor.g : 80,
                           field.focused ? kHighlightColor.b : 80,
                           255);
    SDL_RenderDrawRect(renderer_, &field.rect);
    DrawText(field.value, field.rect.x + 8, field.rect.y + 7, kTextColor);
}

void TemplateEditorModal::DrawSectionHeader(const std::string& label, int x, int y, bool expanded, SDL_Rect* headerRect) {
    if (headerRect) {
        *headerRect = {x - 8, y - 6, contentAreaRect_.w - 24, 26};
    }
    SDL_SetRenderDrawColor(renderer_, 45, 48, 55, 255);
    SDL_Rect bg{ x - 10, y - 6, contentAreaRect_.w - 20, 24 };
    SDL_RenderFillRect(renderer_, &bg);
    SDL_SetRenderDrawColor(renderer_, 70, 75, 85, 255);
    SDL_RenderDrawRect(renderer_, &bg);

    SDL_SetRenderDrawColor(renderer_, kHighlightColor.r, kHighlightColor.g, kHighlightColor.b, 255);
    SDL_Point triangle[3];
    if (expanded) {
        triangle[0] = {x, y + 8};
        triangle[1] = {x + 10, y + 8};
        triangle[2] = {x + 5, y + 16};
    } else {
        triangle[0] = {x + 2, y + 6};
        triangle[1] = {x + 10, y + 10};
        triangle[2] = {x + 2, y + 14};
    }
    SDL_RenderDrawLines(renderer_, triangle, 3);
    SDL_RenderDrawLine(renderer_, triangle[2].x, triangle[2].y, triangle[0].x, triangle[0].y);

    DrawText(label, x + 14, y - 2, kHighlightColor);
}

void TemplateEditorModal::RenderCheckbox(int x, int y) {
    checkboxRect_ = {x, y, 22, 22};
    SDL_SetRenderDrawColor(renderer_, 40, 45, 52, 255);
    SDL_RenderFillRect(renderer_, &checkboxRect_);
    SDL_SetRenderDrawColor(renderer_, 90, 95, 105, 255);
    SDL_RenderDrawRect(renderer_, &checkboxRect_);
    if (applyToExisting_) {
        SDL_SetRenderDrawColor(renderer_, kHighlightColor.r, kHighlightColor.g, kHighlightColor.b, 255);
        SDL_Rect inner{checkboxRect_.x + 4, checkboxRect_.y + 4, checkboxRect_.w - 8, checkboxRect_.h - 8};
        SDL_RenderFillRect(renderer_, &inner);
    }
    DrawText("Apply changes to current template", checkboxRect_.x + 32, checkboxRect_.y, kTextColor);
}

void TemplateEditorModal::ComputeLayout(int windowWidth, int windowHeight) {
    const int panelWidth = 560;
    const int panelHeight = 640;
    panelRect_.w = panelWidth;
    panelRect_.h = panelHeight;
    panelRect_.x = std::max(0, (windowWidth - panelWidth) / 2);
    panelRect_.y = std::max(0, (windowHeight - panelHeight) / 2);

    const int buttonWidth = 150;
    const int buttonHeight = 40;
    saveButtonRect_ = {panelRect_.x + panelRect_.w - buttonWidth - 20, panelRect_.y + panelRect_.h - 60, buttonWidth, buttonHeight};
    cancelButtonRect_ = {panelRect_.x + 20, panelRect_.y + panelRect_.h - 60, buttonWidth, buttonHeight};
}

void TemplateEditorModal::AdjustScroll(int delta) {
    if (maxContentScroll_ <= 0) {
        contentScroll_ = 0;
        return;
    }
    contentScroll_ = std::clamp(contentScroll_ + delta, 0, maxContentScroll_);
}

void TemplateEditorModal::UpdateScrollBounds(int totalContentHeight) {
    maxContentScroll_ = std::max(0, totalContentHeight - contentAreaRect_.h);
    contentScroll_ = std::clamp(contentScroll_, 0, maxContentScroll_);
}

bool TemplateEditorModal::ToggleSectionAt(int x, int y) {
    if (PointInRect(x, y, bodyHeaderRect_)) {
        bodyExpanded_ = !bodyExpanded_;
        return true;
    }
    if (PointInRect(x, y, spriteHeaderRect_)) {
        spriteExpanded_ = !spriteExpanded_;
        return true;
    }
    return false;
}

}  // namespace level_editor


