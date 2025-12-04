#include "level_editor/ui/ComponentEditorPanel.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "level_editor/AssetCache.h"
#include "level_editor/LevelState.h"
#include "level_editor/SchemaService.h"

namespace level_editor {

namespace {

constexpr SDL_Color kPanelBg{26, 28, 33, 255};
constexpr SDL_Color kHeaderColor{34, 36, 42, 255};
constexpr SDL_Color kListBg{32, 34, 40, 255};
constexpr SDL_Color kEditorBg{30, 32, 38, 255};
constexpr SDL_Color kAccent{255, 210, 64, 255};
constexpr SDL_Color kTextColor{225, 226, 232, 255};
constexpr SDL_Color kMutedText{170, 174, 182, 255};
constexpr SDL_Color kDanger{214, 96, 86, 255};
constexpr SDL_Color kTemplateValueColor{154, 182, 214, 255};
constexpr int kPanelPadding = 16;
constexpr int kSectionGap = 16;
constexpr int kHeaderHeight = 148;
constexpr int kFieldControlHeight = 38;
constexpr int kFieldVerticalGap = 18;
constexpr int kListMinHeight = 140;
constexpr int kAddMenuPadding = 10;
constexpr int kAddMenuRowHeight = 28;
constexpr int kAddMenuRowGap = 4;
constexpr int kAddMenuMaxHeight = 320;
constexpr int kSpecialButtonWidth = 170;
constexpr int kSpecialButtonHeight = 34;
constexpr int kSpecialButtonGap = 10;

bool PointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

std::string FormatFloat(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << value;
    auto result = oss.str();
    if (auto dot = result.find('.'); dot != std::string::npos) {
        while (!result.empty() && result.back() == '0') {
            result.pop_back();
        }
        if (!result.empty() && result.back() == '.') {
            result.pop_back();
        }
    }
    return result;
}

std::string JoinPath(const std::vector<std::string>& path) {
    std::ostringstream oss;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            oss << '.';
        }
        oss << path[i];
    }
    return oss.str();
}

std::string MakeComponentKey(const std::string& type, size_t index) {
    return type + "#" + std::to_string(index);
}

bool SplitComponentKey(const std::string& key, std::string& typeOut, size_t& indexOut) {
    const auto hash = key.find('#');
    if (hash == std::string::npos) {
        return false;
    }
    typeOut = key.substr(0, hash);
    try {
        indexOut = std::stoul(key.substr(hash + 1));
    } catch (...) {
        return false;
    }
    return true;
}

bool ValuesEqual(const nlohmann::json* a, const nlohmann::json* b) {
    if (!a && !b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return *a == *b;
}

bool IsVector2Array(const nlohmann::json& value) {
    return value.is_array() && value.size() == 2 && value[0].is_number() && value[1].is_number();
}

bool IsStringArray(const nlohmann::json& value) {
    if (!value.is_array()) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const nlohmann::json& entry) { return entry.is_string(); });
}

std::string ComponentDisplayName(const SchemaService* schemaService, const std::string& type) {
    if (!schemaService) {
        return type;
    }
    if (const auto* descriptor = schemaService->GetComponentDescriptor(type)) {
        return descriptor->displayName.empty() ? type : descriptor->displayName;
    }
    return type;
}

struct FieldKeyInfo {
    std::string metaId;
    std::string suffix;
};

FieldKeyInfo DecodeFieldKey(const std::string& key) {
    FieldKeyInfo info;
    if (const auto pipe = key.rfind('|'); pipe != std::string::npos) {
        info.metaId = key.substr(0, pipe);
        info.suffix = key.substr(pipe + 1);
    } else {
        info.metaId = key;
    }
    return info;
}

std::string Trim(std::string value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
    return value;
}

}  // namespace

const nlohmann::json* ResolveTemplateValue(const nlohmann::json* root, const std::vector<std::string>& path) {
    if (!root) {
        return nullptr;
    }
    const nlohmann::json* current = root;
    for (const auto& segment : path) {
        if (!current->is_object()) {
            return nullptr;
        }
        auto it = current->find(segment);
        if (it == current->end()) {
            return nullptr;
        }
        current = &(*it);
    }
    return current;
}

void ComponentEditorPanel::TextField::SetValue(const std::string& text) {
    value = text;
    cursor = value.size();
}

bool ComponentEditorPanel::TextField::HandleMouseDown(int x, int y, TTF_Font* font) {
    if (!PointInRect(x, y, rect)) {
        return false;
    }
    focused = true;
    if (!font || value.empty()) {
        cursor = value.size();
        return true;
    }
    const int padding = 6;
    const int relativeX = x - (rect.x + padding);
    if (relativeX <= 0) {
        cursor = 0;
        return true;
    }
    cursor = value.size();
    std::string prefix;
    prefix.reserve(value.size());
    int width = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        prefix.push_back(value[i]);
        if (TTF_SizeUTF8(font, prefix.c_str(), &width, nullptr) != 0) {
            continue;
        }
        if (width >= relativeX) {
            cursor = i + 1;
            break;
        }
    }
    return true;
}

bool ComponentEditorPanel::TextField::HandleTextInput(const SDL_TextInputEvent& evt) {
    if (!focused) {
        return false;
    }
    value.insert(cursor, evt.text);
    cursor += SDL_strlen(evt.text);
    return true;
}

bool ComponentEditorPanel::TextField::HandleKeyDown(const SDL_KeyboardEvent& evt) {
    if (!focused) {
        return false;
    }
    switch (evt.keysym.sym) {
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

void ComponentEditorPanel::TextField::Defocus() {
    focused = false;
}

ComponentEditorPanel::ComponentEditorPanel() = default;

void ComponentEditorPanel::Initialize(SDL_Renderer* renderer,
                                      TTF_Font* font,
                                      LevelState* levelState,
                                      const SchemaService* schemaService,
                                      AssetCache* assetCache) {
    renderer_ = renderer;
    font_ = font;
    levelState_ = levelState;
    schemaService_ = schemaService;
    assetCache_ = assetCache;
}

void ComponentEditorPanel::SetCallbacks(Callbacks callbacks) {
    callbacks_ = std::move(callbacks);
}

void ComponentEditorPanel::ForceRefresh() {
    selectionCache_.clear();
    activeObjectName_.clear();
    activeComponentKey_.clear();
    fieldStates_.clear();
    componentSelection_.clear();
    currentFields_.clear();
    ClearHeaderFocus(false);
    objectNameField_.SetValue("");
    templateField_.SetValue("");
}

void ComponentEditorPanel::BeginTextInput() {
    if (!textInputActive_) {
        SDL_StartTextInput();
        textInputActive_ = true;
    }
}

void ComponentEditorPanel::EndTextInput() {
    if (textInputActive_) {
        SDL_StopTextInput();
        textInputActive_ = false;
    }
}

void ComponentEditorPanel::Render(const SDL_Rect& bounds) {
    if (!renderer_) {
        return;
    }

    SDL_SetRenderDrawColor(renderer_, kPanelBg.r, kPanelBg.g, kPanelBg.b, kPanelBg.a);
    SDL_RenderFillRect(renderer_, &bounds);

    SyncSelection();

    componentRowRects_.clear();
    removeButtonRects_.clear();
    duplicateButtonRects_.clear();
    fieldRects_.clear();
    resetButtonRects_.clear();
    boolToggleRects_.clear();
    enumButtonRects_.clear();
    specialActionButtons_.clear();
    addMenuEntryRects_.clear();
    templateRowData_.clear();

    const int contentX = bounds.x + kPanelPadding;
    const int contentWidth = std::max(0, bounds.w - kPanelPadding * 2);
    SDL_Rect headerRect{contentX, bounds.y + kPanelPadding, contentWidth, kHeaderHeight};
    headerArea_ = headerRect;
    const int listHeight = std::max(kListMinHeight, (bounds.h - kHeaderHeight - kPanelPadding * 3) / 2);
    listArea_ = {contentX, headerRect.y + headerRect.h + kSectionGap, contentWidth, listHeight};
    const int editorTop = listArea_.y + listArea_.h + kSectionGap;
    const int editorHeight = std::max(140, bounds.y + bounds.h - editorTop - kPanelPadding);
    editorArea_ = {contentX, editorTop, contentWidth, editorHeight};

    RenderHeader(headerRect);
    RenderComponentList(listArea_);
    RenderEditor(editorArea_);
    RenderAddMenu(bounds);
}

bool ComponentEditorPanel::HandleEvent(const SDL_Event& event, const SDL_Rect& bounds) {
    if (HandleTextInputEvent(event)) {
        return true;
    }
    if (HandleKeyEvent(event)) {
        return true;
    }
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return HandleMouseButton(event, bounds);
        case SDL_MOUSEWHEEL:
            return HandleMouseWheel(event, bounds);
        default:
            break;
    }
    return false;
}

void ComponentEditorPanel::RenderHeader(const SDL_Rect& bounds) {
    SDL_SetRenderDrawColor(renderer_, kHeaderColor.r, kHeaderColor.g, kHeaderColor.b, kHeaderColor.a);
    SDL_RenderFillRect(renderer_, &bounds);
    SDL_SetRenderDrawColor(renderer_, 45, 48, 58, 255);
    SDL_RenderDrawRect(renderer_, &bounds);

    const bool hasSelection = !activeObjectName_.empty() && FindSelectedObject() != nullptr;
    const SDL_Color labelColor = hasSelection ? kMutedText : SDL_Color{110, 115, 125, 255};
    const std::string title = hasSelection ? "Editing Object" : "Component Editor";
    DrawText(title, bounds.x + 12, bounds.y + 12, kTextColor);

    const int buttonWidth = 160;
    const int buttonHeight = 40;
    const int fieldWidth = std::max(120, bounds.w - buttonWidth - 24);
    const int fieldLeft = bounds.x + 12;

    auto drawHeaderField = [&](const std::string& label,
                               TextField& field,
                               HeaderFocus focus,
                               HeaderFocus currentFocus,
                               const std::string& placeholder,
                               bool enabled) {
        DrawText(label, fieldLeft, field.rect.y - 18, labelColor);
        SDL_Color frameColor = (focus == currentFocus && enabled) ? kAccent : SDL_Color{70, 74, 82, 255};
        SDL_Color bgColor = enabled ? SDL_Color{45, 48, 58, 255} : SDL_Color{38, 40, 46, 255};
        SDL_SetRenderDrawColor(renderer_, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(renderer_, &field.rect);
        SDL_SetRenderDrawColor(renderer_, frameColor.r, frameColor.g, frameColor.b, frameColor.a);
        SDL_RenderDrawRect(renderer_, &field.rect);
        const std::string& text = field.value.empty() ? placeholder : field.value;
        const SDL_Color textColor = enabled ? kTextColor : SDL_Color{120, 124, 132, 255};
        DrawText(text, field.rect.x + 8, field.rect.y + 7, textColor);
    };

    objectNameField_.rect = {fieldLeft, bounds.y + 52, fieldWidth, 32};
    templateField_.rect = {fieldLeft, bounds.y + 104, fieldWidth, 32};

    drawHeaderField("Object Name",
                    objectNameField_,
                    HeaderFocus::kObjectName,
                    headerFocus_,
                    hasSelection ? "<unnamed>" : "No object selected",
                    hasSelection);
    drawHeaderField("Template ID",
                    templateField_,
                    HeaderFocus::kTemplate,
                    headerFocus_,
                    hasSelection ? "<none>" : "Select an object to edit",
                    hasSelection);

    addButtonRect_ = {bounds.x + bounds.w - buttonWidth - 12, bounds.y + 32, buttonWidth, buttonHeight};
    if (hasSelection) {
        SDL_SetRenderDrawColor(renderer_, kAccent.r, kAccent.g, kAccent.b, 255);
        SDL_RenderFillRect(renderer_, &addButtonRect_);
        SDL_SetRenderDrawColor(renderer_, 45, 34, 14, 255);
        SDL_RenderDrawRect(renderer_, &addButtonRect_);
        DrawText("+ Add Component", addButtonRect_.x + 16, addButtonRect_.y + 11, SDL_Color{25, 18, 6, 255});
    } else {
        SDL_SetRenderDrawColor(renderer_, 60, 64, 70, 255);
        SDL_RenderFillRect(renderer_, &addButtonRect_);
        SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
        SDL_RenderDrawRect(renderer_, &addButtonRect_);
        DrawText("Select object first", addButtonRect_.x + 10, addButtonRect_.y + 12, SDL_Color{150, 150, 150, 255});
    }
}

void ComponentEditorPanel::RenderComponentList(const SDL_Rect& bounds) {
    SDL_SetRenderDrawColor(renderer_, kListBg.r, kListBg.g, kListBg.b, kListBg.a);
    SDL_RenderFillRect(renderer_, &bounds);
    SDL_SetRenderDrawColor(renderer_, 45, 48, 58, 255);
    SDL_RenderDrawRect(renderer_, &bounds);

    nlohmann::json* object = FindSelectedObject();
    if (!object || !object->contains("components") || !(*object)["components"].is_array()) {
        DrawText("Select an object with components to edit.", bounds.x + 10, bounds.y + 10, kMutedText);
        return;
    }

    const auto& components = (*object)["components"];
    std::unordered_map<std::string, int> actualTypeCounts;
    if (components.empty()) {
        SDL_SetRenderDrawColor(renderer_, 38, 40, 46, 255);
        SDL_Rect emptyRect{bounds.x + 10, bounds.y + 10, bounds.w - 20, 42};
        SDL_RenderFillRect(renderer_, &emptyRect);
        DrawText("Template components only. Select one to create an override.", bounds.x + 14, bounds.y + 22, kMutedText);
    }

    SDL_Rect clip{bounds.x + 4, bounds.y + 4, bounds.w - 8, bounds.h - 8};
    SDL_RenderSetClipRect(renderer_, &clip);

    const int rowHeight = 60;
    int yOffset = clip.y - listScroll_;

    for (size_t i = 0; i < components.size(); ++i) {
        const auto& component = components.at(i);
        const std::string type = component.value("type", std::string("Unknown"));
        const std::string componentKey = MakeComponentKey(type, i);
        const bool selected = componentKey == activeComponentKey_;
        actualTypeCounts[type]++;
        SDL_Rect row{clip.x + 6, yOffset, clip.w - 12, rowHeight - 12};
        yOffset += rowHeight;

        if (row.y + row.h < clip.y || row.y > clip.y + clip.h) {
            continue;
        }

        SDL_Color rowColor = selected ? SDL_Color{55, 92, 140, 255} : SDL_Color{40, 42, 48, 255};
        SDL_SetRenderDrawColor(renderer_, rowColor.r, rowColor.g, rowColor.b, rowColor.a);
        SDL_RenderFillRect(renderer_, &row);
        SDL_SetRenderDrawColor(renderer_, 70, 74, 82, 255);
        SDL_RenderDrawRect(renderer_, &row);

        const std::string displayName = ComponentDisplayName(schemaService_, type);
        DrawText(displayName, row.x + 12, row.y + 10, kTextColor);

        SDL_Rect duplicateRect{row.x + row.w - 72, row.y + 10, 28, 28};
        SDL_Rect removeRect{row.x + row.w - 38, row.y + 10, 28, 28};

        SDL_SetRenderDrawColor(renderer_, 60, 64, 72, 255);
        SDL_RenderFillRect(renderer_, &duplicateRect);
        SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
        SDL_RenderDrawRect(renderer_, &duplicateRect);
        DrawText("⎘", duplicateRect.x + 7, duplicateRect.y + 4, kTextColor);

        SDL_SetRenderDrawColor(renderer_, kDanger.r, kDanger.g, kDanger.b, 255);
        SDL_RenderFillRect(renderer_, &removeRect);
        SDL_SetRenderDrawColor(renderer_, 40, 18, 18, 255);
        SDL_RenderDrawRect(renderer_, &removeRect);
        DrawText("×", removeRect.x + 8, removeRect.y + 4, SDL_Color{255, 255, 255, 255});

        componentRowRects_.push_back({componentKey, row});
        duplicateButtonRects_.push_back({componentKey, duplicateRect});
        removeButtonRects_.push_back({componentKey, removeRect});
    }

    if (schemaService_) {
        auto* object = FindSelectedObject();
        const std::string templateId = object ? object->value("template", std::string{}) : std::string{};
        if (!templateId.empty()) {
            if (const auto* templateDesc = schemaService_->GetTemplate(templateId)) {
                std::unordered_map<std::string, int> matchedCounts;
                std::unordered_map<std::string, int> extraCounts;
                for (const auto& tmplComponent : templateDesc->componentDefaults) {
                    const std::string type = tmplComponent.value("type", std::string{});
                    if (type.empty()) {
                        continue;
                    }
                    const int actualCount = actualTypeCounts[type];
                    if (matchedCounts[type] < actualCount) {
                        matchedCounts[type]++;
                        continue;
                    }
                    const int newIndex = actualCount + extraCounts[type];
                    extraCounts[type]++;
                    std::string templateKey = MakeComponentKey(type, newIndex);
                    templateRowData_[templateKey] = tmplComponent;

                    SDL_Rect row{clip.x + 6, yOffset, clip.w - 12, rowHeight - 12};
                    yOffset += rowHeight;
                    if (row.y + row.h < clip.y || row.y > clip.y + clip.h) {
                        continue;
                    }
                    SDL_Color rowColor = SDL_Color{35, 38, 44, 255};
                    SDL_SetRenderDrawColor(renderer_, rowColor.r, rowColor.g, rowColor.b, rowColor.a);
                    SDL_RenderFillRect(renderer_, &row);
                    SDL_SetRenderDrawColor(renderer_, 58, 62, 70, 255);
                    SDL_RenderDrawRect(renderer_, &row);

                    const std::string displayName = ComponentDisplayName(schemaService_, type);
                    DrawText(displayName, row.x + 12, row.y + 10, kTemplateValueColor);

                    componentRowRects_.push_back({templateKey, row});
                }
            }
        }
    }

    SDL_RenderSetClipRect(renderer_, nullptr);

    const int totalRows = static_cast<int>(components.size() + templateRowData_.size());
    const int totalHeight = totalRows * rowHeight;
    const int maxScroll = std::max(0, totalHeight - clip.h);
    listScroll_ = std::clamp(listScroll_, 0, maxScroll);
}

void ComponentEditorPanel::RenderEditor(const SDL_Rect& bounds) {
    SDL_SetRenderDrawColor(renderer_, kEditorBg.r, kEditorBg.g, kEditorBg.b, kEditorBg.a);
    SDL_RenderFillRect(renderer_, &bounds);
    SDL_SetRenderDrawColor(renderer_, 45, 48, 58, 255);
    SDL_RenderDrawRect(renderer_, &bounds);

    nlohmann::json* component = FindActiveComponent();
    if (!component) {
        DrawText("Select a component to see its properties.", bounds.x + 10, bounds.y + 10, kMutedText);
        return;
    }

    RebuildFields();

    SDL_Rect clip = bounds;
    SDL_RenderSetClipRect(renderer_, &clip);

    const int contentLeft = bounds.x + 12;
    const int contentRight = bounds.x + bounds.w - 12;
    const int contentWidth = contentRight - contentLeft;
    int y = bounds.y + 12 - editorScroll_;

    const std::string type = component->value("type", std::string("Unknown"));
    DrawText(ComponentDisplayName(schemaService_, type), contentLeft, y, kTextColor);
    y += 32;

    for (auto& meta : currentFields_) {
        auto& state = fieldStates_[meta.id];
        RenderField(meta, state, contentLeft, y, contentWidth);
        y += kFieldControlHeight + kFieldVerticalGap + 24;
    }

    RenderSpecialComponentUI(bounds);

    SDL_RenderSetClipRect(renderer_, nullptr);

    const int totalHeight = y - (bounds.y + 12) + editorScroll_;
    const int maxScroll = std::max(0, totalHeight - bounds.h + 16);
    editorScroll_ = std::clamp(editorScroll_, 0, maxScroll);
}

void ComponentEditorPanel::RenderAddMenu(const SDL_Rect& bounds) {
    addMenuEntryRects_.clear();
    if (!addMenu_.open || addMenu_.componentTypes.empty()) {
        addMenu_.visibleRows = 0;
        return;
    }

    const int menuWidth = std::min(320, bounds.w - kPanelPadding * 2);
    int menuX = std::clamp(addButtonRect_.x + addButtonRect_.w - menuWidth,
                           bounds.x + kPanelPadding,
                           bounds.x + bounds.w - kPanelPadding - menuWidth);
    int menuY = addButtonRect_.y + addButtonRect_.h + 8;
    int availableBelow = bounds.y + bounds.h - menuY - kPanelPadding;
    int menuHeight = std::min(kAddMenuMaxHeight, std::max(kAddMenuRowHeight + kAddMenuPadding * 2, availableBelow));
    if (menuY + menuHeight > bounds.y + bounds.h - kPanelPadding) {
        menuY = addButtonRect_.y - menuHeight - 8;
        if (menuY < bounds.y + kPanelPadding) {
            menuY = bounds.y + kPanelPadding;
        }
    }

    addMenu_.rect = {menuX, menuY, menuWidth, menuHeight};

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 20, 20, 24, 225);
    SDL_RenderFillRect(renderer_, &addMenu_.rect);
    SDL_SetRenderDrawColor(renderer_, 70, 74, 82, 255);
    SDL_RenderDrawRect(renderer_, &addMenu_.rect);

    const int viewportHeight = addMenu_.rect.h - kAddMenuPadding * 2;
    const int rowsPerView = std::max(1, viewportHeight / (kAddMenuRowHeight + kAddMenuRowGap));
    addMenu_.visibleRows = rowsPerView;
    const int maxScroll = std::max(0, static_cast<int>(addMenu_.componentTypes.size()) - rowsPerView);
    addMenu_.scroll = std::clamp(addMenu_.scroll, 0, maxScroll);

    SDL_Rect clip{addMenu_.rect.x + 2, addMenu_.rect.y + 2, addMenu_.rect.w - 4, addMenu_.rect.h - 4};
    SDL_RenderSetClipRect(renderer_, &clip);

    int y = addMenu_.rect.y + kAddMenuPadding;
    for (int idx = addMenu_.scroll; idx < static_cast<int>(addMenu_.componentTypes.size()); ++idx) {
        if (y + kAddMenuRowHeight > addMenu_.rect.y + addMenu_.rect.h - kAddMenuPadding) {
            break;
        }
        SDL_Rect row{addMenu_.rect.x + kAddMenuPadding, y, addMenu_.rect.w - kAddMenuPadding * 2, kAddMenuRowHeight};
        SDL_SetRenderDrawColor(renderer_, 40, 44, 52, 255);
        SDL_RenderFillRect(renderer_, &row);
        SDL_SetRenderDrawColor(renderer_, 55, 58, 66, 255);
        SDL_RenderDrawRect(renderer_, &row);
        DrawText(addMenu_.componentTypes[idx], row.x + 6, row.y + 6, kTextColor);
        addMenuEntryRects_.push_back({"add:" + addMenu_.componentTypes[idx], row});
        y += kAddMenuRowHeight + kAddMenuRowGap;
    }

    SDL_RenderSetClipRect(renderer_, nullptr);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

bool ComponentEditorPanel::HandleMouseButton(const SDL_Event& event, const SDL_Rect& bounds) {
    if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP) {
        return false;
    }
    const bool isDown = event.type == SDL_MOUSEBUTTONDOWN;
    const int mouseX = event.button.x;
    const int mouseY = event.button.y;

    if (isDown && dropdown_.open && !PointInRect(mouseX, mouseY, dropdown_.rect)) {
        CloseDropdown();
    }

    if (isDown && PointInRect(mouseX, mouseY, addButtonRect_) && event.button.button == SDL_BUTTON_LEFT) {
        ClearFocus();
        if (!FindSelectedObject()) {
            return true;
        }
        if (schemaService_) {
            addMenu_.open = true;
            addMenu_.componentTypes = schemaService_->ListComponentTypes();
            addMenu_.scroll = 0;
        }
        return true;
    }

    if (addMenu_.open && PointInRect(mouseX, mouseY, addMenu_.rect)) {
        if (isDown && event.button.button == SDL_BUTTON_LEFT) {
            for (const auto& entry : addMenuEntryRects_) {
                if (PointInRect(mouseX, mouseY, entry.second)) {
                    AddComponent(entry.first.substr(4));
                    addMenu_.open = false;
                    return true;
                }
            }
        }
        return false;
    }

    if (isDown && event.button.button == SDL_BUTTON_LEFT && PointInRect(mouseX, mouseY, headerArea_)) {
        if (HandleHeaderMouseDown(mouseX, mouseY)) {
            return true;
        }
        ClearFocus();
        return true;
    }

    if (!PointInRect(mouseX, mouseY, bounds)) {
        if (isDown && addMenu_.open) {
            addMenu_.open = false;
        }
        if (isDown && event.button.button == SDL_BUTTON_LEFT) {
            ClearFocus();
        }
        return false;
    }

    if (isDown && addMenu_.open) {
        addMenu_.open = false;
    }

    if (isDown) {
        const bool isLeftButton = event.button.button == SDL_BUTTON_LEFT;

        if (isLeftButton) {
            for (const auto& [key, rect] : removeButtonRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    RemoveComponent(key);
                    return true;
                }
            }

            for (const auto& [key, rect] : duplicateButtonRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    DuplicateComponent(key);
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [key, rect] : componentRowRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    if (templateRowData_.count(key) && !InstantiateTemplateComponent(key)) {
                        return true;
                    }
                    ClearFocus();
                    if (key != activeComponentKey_) {
                        activeComponentKey_ = key;
                        currentFields_.clear();
                    }
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [key, rect] : fieldRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    focusedFieldId_ = key;
                    const auto decoded = DecodeFieldKey(key);
                    auto it = fieldStates_.find(decoded.metaId);
                    if (it != fieldStates_.end()) {
                        TextField* field = &it->second.primary;
                        if (decoded.suffix == "y") {
                            field = &it->second.secondary;
                        }
                        if (field) {
                            field->HandleMouseDown(mouseX, mouseY, font_);
                            BeginTextInput();
                        }
                    }
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [key, rect] : boolToggleRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    auto it = fieldStates_.find(key);
                    if (it != fieldStates_.end()) {
                        it->second.boolValue = !it->second.boolValue;
                        auto metaIt = std::find_if(currentFields_.begin(), currentFields_.end(),
                                                   [&](const FieldMeta& meta) { return meta.id == key; });
                        if (metaIt != currentFields_.end()) {
                            ApplyBoolChange(*metaIt, it->second);
                        }
                    }
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [key, rect] : enumButtonRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    auto metaIt =
                        std::find_if(currentFields_.begin(), currentFields_.end(), [&](const FieldMeta& m) { return m.id == key; });
                    if (metaIt != currentFields_.end()) {
                        ToggleDropdown(key, rect, metaIt->descriptor.enumValues);
                    }
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [key, rect] : resetButtonRects_) {
                if (PointInRect(mouseX, mouseY, rect)) {
                    ClearFocus();
                    auto metaIt =
                        std::find_if(currentFields_.begin(), currentFields_.end(), [&](const FieldMeta& m) { return m.id == key; });
                    if (metaIt != currentFields_.end()) {
                        ResetFieldToTemplate(*metaIt);
                    }
                    return true;
                }
            }
        }

        if (isLeftButton) {
            for (const auto& [action, rect] : specialActionButtons_) {
                if (!PointInRect(mouseX, mouseY, rect)) {
                    continue;
                }
                ClearFocus();
                auto* component = FindActiveComponent();
                if (!component) {
                    return true;
                }
                if (action == "sensor:box") {
                    nlohmann::json payload;
                    if (component->contains("boxZone")) {
                        payload = (*component)["boxZone"];
                    }
                    TriggerMapInteraction(MapInteractionType::kSensorBox, payload);
                } else if (action == "rail:path") {
                    if (component->contains("path")) {
                        TriggerMapInteraction(MapInteractionType::kRailPath, (*component)["path"]);
                    } else {
                        TriggerMapInteraction(MapInteractionType::kRailPath, nlohmann::json::array());
                    }
                } else if (action == "joint:anchors") {
                    nlohmann::json payload = *component;
                    payload["objectName"] = activeObjectName_;
                    TriggerMapInteraction(MapInteractionType::kJointAnchors, payload);
                } else if (action == "spawner:locations") {
                    if (component->contains("spawnLocations")) {
                        TriggerMapInteraction(MapInteractionType::kSpawnerLocations, (*component)["spawnLocations"]);
                    } else {
                        TriggerMapInteraction(MapInteractionType::kSpawnerLocations, nlohmann::json::array());
                    }
                }
                return true;
            }
        }
    }

    return false;
}

bool ComponentEditorPanel::HandleHeaderMouseDown(int x, int y) {
    auto* object = FindSelectedObject();
    if (!object) {
        ClearHeaderFocus(false);
        return false;
    }
    if (PointInRect(x, y, objectNameField_.rect)) {
        ClearFocus();
        headerFocus_ = HeaderFocus::kObjectName;
        objectNameField_.HandleMouseDown(x, y, font_);
        BeginTextInput();
        return true;
    }
    if (PointInRect(x, y, templateField_.rect)) {
        ClearFocus();
        headerFocus_ = HeaderFocus::kTemplate;
        templateField_.HandleMouseDown(x, y, font_);
        BeginTextInput();
        return true;
    }
    return false;
}

bool ComponentEditorPanel::HandleMouseWheel(const SDL_Event& event, const SDL_Rect& bounds) {
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    if (addMenu_.open && PointInRect(mouseX, mouseY, addMenu_.rect)) {
        const int maxScroll = std::max(0, static_cast<int>(addMenu_.componentTypes.size()) - addMenu_.visibleRows);
        addMenu_.scroll = std::clamp(addMenu_.scroll - event.wheel.y, 0, maxScroll);
        return true;
    }
    if (PointInRect(mouseX, mouseY, listArea_)) {
        listScroll_ = std::max(0, listScroll_ - event.wheel.y * 24);
        return true;
    }
    if (PointInRect(mouseX, mouseY, editorArea_)) {
        editorScroll_ = std::max(0, editorScroll_ - event.wheel.y * 32);
        return true;
    }
    return false;
}

bool ComponentEditorPanel::HandleTextInputEvent(const SDL_Event& event) {
    if (event.type != SDL_TEXTINPUT) {
        return false;
    }
    if (HandleHeaderTextInput(event.text)) {
        return true;
    }
    if (focusedFieldId_.empty()) {
        return false;
    }
    const auto decoded = DecodeFieldKey(focusedFieldId_);
    auto stateIt = fieldStates_.find(decoded.metaId);
    if (stateIt == fieldStates_.end()) {
        return false;
    }
    auto metaIt =
        std::find_if(currentFields_.begin(), currentFields_.end(), [&](const FieldMeta& m) { return m.id == decoded.metaId; });
    if (metaIt == currentFields_.end()) {
        return false;
    }
    TextField* field = &stateIt->second.primary;
    bool isPrimary = true;
    if (decoded.suffix == "y") {
        field = &stateIt->second.secondary;
        isPrimary = false;
    }
    if (field && field->HandleTextInput(event.text)) {
        if (metaIt->kind == FieldKind::kVector2) {
            ApplyVectorChange(*metaIt, stateIt->second, isPrimary);
        } else if (metaIt->kind == FieldKind::kStringList) {
            ApplyListChange(*metaIt, stateIt->second);
        } else {
            ApplyFieldChange(*metaIt, stateIt->second);
        }
        return true;
    }
    return false;
}

bool ComponentEditorPanel::HandleHeaderTextInput(const SDL_TextInputEvent& event) {
    if (headerFocus_ == HeaderFocus::kNone) {
        return false;
    }
    TextField* field = headerFocus_ == HeaderFocus::kObjectName ? &objectNameField_ : &templateField_;
    return field && field->HandleTextInput(event);
}

bool ComponentEditorPanel::HandleKeyEvent(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) {
        return false;
    }
    if (HandleHeaderKeyEvent(event)) {
        return true;
    }
    if (focusedFieldId_.empty()) {
        return false;
    }
    const auto decoded = DecodeFieldKey(focusedFieldId_);
    auto stateIt = fieldStates_.find(decoded.metaId);
    if (stateIt == fieldStates_.end()) {
        return false;
    }
    auto metaIt =
        std::find_if(currentFields_.begin(), currentFields_.end(), [&](const FieldMeta& m) { return m.id == decoded.metaId; });
    if (metaIt == currentFields_.end()) {
        return false;
    }
    TextField* field = &stateIt->second.primary;
    bool isPrimary = true;
    if (decoded.suffix == "y") {
        field = &stateIt->second.secondary;
        isPrimary = false;
    }
    if (field && field->HandleKeyDown(event.key)) {
        if (metaIt->kind == FieldKind::kVector2) {
            ApplyVectorChange(*metaIt, stateIt->second, isPrimary);
        } else if (metaIt->kind == FieldKind::kStringList) {
            ApplyListChange(*metaIt, stateIt->second);
        } else {
            ApplyFieldChange(*metaIt, stateIt->second);
        }
        return true;
    }
    return false;
}

bool ComponentEditorPanel::HandleHeaderKeyEvent(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN || headerFocus_ == HeaderFocus::kNone) {
        return false;
    }
    TextField* field = headerFocus_ == HeaderFocus::kObjectName ? &objectNameField_ : &templateField_;
    if (!field || !field->focused) {
        return false;
    }
    const SDL_Keycode key = event.key.keysym.sym;
    if (field->HandleKeyDown(event.key)) {
        if (key == SDLK_RETURN) {
            ClearHeaderFocus(true);
        }
        return true;
    }
    return false;
}

void ComponentEditorPanel::SyncSelection() {
    if (!levelState_) {
        return;
    }
    const auto& selection = levelState_->GetSelection();
    if (selection == selectionCache_) {
        return;
    }
    selectionCache_ = selection;
    if (selection.empty()) {
        activeObjectName_.clear();
        activeComponentKey_.clear();
        currentFields_.clear();
        fieldStates_.clear();
        SyncHeaderFields();
        return;
    }
    activeObjectName_ = selection.front();
    activeComponentKey_.clear();
    currentFields_.clear();
    fieldStates_.clear();
    SyncHeaderFields();
}

void ComponentEditorPanel::SyncHeaderFields() {
    ClearHeaderFocus(false);
    auto* object = FindSelectedObject();
    if (object) {
        objectNameField_.SetValue(object->value("name", activeObjectName_));
        templateField_.SetValue(object->value("template", std::string{}));
    } else {
        objectNameField_.SetValue("");
        templateField_.SetValue("");
    }
}

nlohmann::json* ComponentEditorPanel::FindSelectedObject() {
    if (!levelState_) {
        return nullptr;
    }
    auto& doc = levelState_->GetDocument().data;
    auto objectsIt = doc.find("objects");
    if (objectsIt == doc.end() || !objectsIt->is_array()) {
        return nullptr;
    }
    for (auto& object : *objectsIt) {
        if (object.value("name", std::string{}) == activeObjectName_) {
            return &object;
        }
    }
    return nullptr;
}

nlohmann::json* ComponentEditorPanel::FindActiveComponent() {
    auto* object = FindSelectedObject();
    if (!object || !object->contains("components") || !(*object)["components"].is_array()) {
        return nullptr;
    }
    auto& components = (*object)["components"];
    if (components.empty()) {
        return nullptr;
    }
    if (activeComponentKey_.empty()) {
        activeComponentKey_ = MakeComponentKey(components.front().value("type", std::string("Unknown")), 0);
    }
    std::string type;
    size_t index = 0;
    if (!SplitComponentKey(activeComponentKey_, type, index)) {
        return nullptr;
    }
    if (index >= components.size()) {
        return nullptr;
    }
    return &components.at(index);
}

void ComponentEditorPanel::RebuildFields() {
    currentFields_.clear();
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    std::string type;
    size_t index = 0;
    if (!SplitComponentKey(activeComponentKey_, type, index)) {
        return;
    }
    const nlohmann::json* templateNode = nullptr;
    if (schemaService_ && !activeObjectName_.empty()) {
        auto* object = FindSelectedObject();
        if (object) {
            const std::string templateId = object->value("template", std::string{});
            if (!templateId.empty()) {
                templateNode = schemaService_->GetTemplateComponentDefaults(templateId, type);
            }
        }
    }
    CollectFields(*component, templateNode, activeComponentKey_, {}, currentFields_);
}

const FieldDescriptor* ComponentEditorPanel::FindDescriptor(const std::string& componentType,
                                                            const std::string& fieldName) const {
    if (!schemaService_) {
        return nullptr;
    }
    const auto* descriptor = schemaService_->GetComponentDescriptor(componentType);
    if (!descriptor) {
        return nullptr;
    }
    for (const auto& field : descriptor->fields) {
        if (field.name == fieldName) {
            return &field;
        }
    }
    return nullptr;
}

void ComponentEditorPanel::CollectFields(const nlohmann::json& node,
                                         const nlohmann::json* templateNode,
                                         const std::string& componentKey,
                                         std::vector<std::string> path,
                                         std::vector<FieldMeta>& out) {
    if (!node.is_object()) {
        return;
    }
    std::string componentType;
    size_t index = 0;
    if (!SplitComponentKey(componentKey, componentType, index)) {
        return;
    }

    std::unordered_set<std::string> seenKeys;

    for (auto it = node.begin(); it != node.end(); ++it) {
        const std::string key = it.key();
        if (key == "type") {
            continue;
        }
        seenKeys.insert(key);
        path.push_back(key);
        if (ShouldSkipPath(path, componentType)) {
            path.pop_back();
            continue;
        }
        const auto& value = it.value();
        const nlohmann::json* templateValue = ResolveTemplateValue(templateNode, path);

        if (value.is_object()) {
            CollectFields(value, templateNode, componentKey, path, out);
            path.pop_back();
            continue;
        }

        FieldMeta meta;
        meta.path = path;
        meta.id = MakeFieldKey(componentKey, meta.path);
        const FieldDescriptor* descriptor = FindDescriptor(componentType, path.back());
        if (descriptor) {
            meta.descriptor = *descriptor;
        }
        if (value.is_boolean() || (descriptor && descriptor->type == FieldType::kBoolean)) {
            meta.kind = FieldKind::kBoolean;
        } else if ((descriptor && descriptor->type == FieldType::kEnum && !descriptor->enumValues.empty())) {
            meta.kind = FieldKind::kEnum;
        } else if (IsVector2Array(value)) {
            meta.kind = FieldKind::kVector2;
        } else if (IsStringArray(value)) {
            meta.kind = FieldKind::kStringList;
        } else if (value.is_number()) {
            meta.kind = FieldKind::kNumber;
        } else {
            meta.kind = FieldKind::kString;
        }
        meta.overridesTemplate = templateValue ? !ValuesEqual(&value, templateValue) : true;
        meta.hasTemplateValue = templateValue != nullptr;
        const bool exists = std::any_of(out.begin(), out.end(), [&](const FieldMeta& existing) { return existing.id == meta.id; });
        if (!exists) {
            out.push_back(meta);
        }
        path.pop_back();
    }

    if (schemaService_) {
        if (const auto* descriptor = schemaService_->GetComponentDescriptor(componentType)) {
            for (const auto& field : descriptor->fields) {
                if (field.name == "type" || seenKeys.count(field.name) > 0) {
                    continue;
                }
                path.push_back(field.name);
                if (ShouldSkipPath(path, componentType)) {
                    path.pop_back();
                    continue;
                }
                FieldMeta meta;
                meta.path = path;
                meta.id = MakeFieldKey(componentKey, meta.path);
                meta.descriptor = field;
                meta.kind = FieldKindFromDescriptor(field.type);
                const nlohmann::json* templateValue = ResolveTemplateValue(templateNode, path);
                meta.overridesTemplate = false;
                meta.hasTemplateValue = templateValue != nullptr;
                const bool exists = std::any_of(out.begin(), out.end(), [&](const FieldMeta& existing) { return existing.id == meta.id; });
                if (!exists) {
                    out.push_back(meta);
                }
                path.pop_back();
            }
        }
    }
}

bool ComponentEditorPanel::ShouldSkipPath(const std::vector<std::string>& path, const std::string& componentType) const {
    if (componentType == "RailComponent" && !path.empty() && path.front() == "path") {
        return true;
    }
    if (componentType == "SensorComponent" && path.size() >= 1 && path.front() == "boxZone") {
        return true;
    }
    if (componentType == "JointComponent" && !path.empty() &&
        (path.front() == "anchorA" || path.front() == "anchorB")) {
        return true;
    }
    if (componentType == "ObjectSpawnerComponent" && !path.empty() && path.front() == "spawnLocations") {
        return true;
    }
    return false;
}

ComponentEditorPanel::FieldState& ComponentEditorPanel::GetOrCreateState(const std::string& id,
                                                                         const std::string& initialValue) {
    auto& state = fieldStates_[id];
    if (!state.primaryInitialized) {
        state.primary.SetValue(initialValue);
        state.primaryInitialized = true;
    }
    return state;
}

void ComponentEditorPanel::RenderField(FieldMeta& meta, FieldState& state, int x, int& y, int width) {
    const int labelY = y;
    const std::string label = meta.descriptor.label.empty() ? PathToLabel(meta.path) : meta.descriptor.label;
    DrawText(label, x, labelY, kMutedText);
    const int controlY = labelY + 22;
    const int controlHeight = kFieldControlHeight;
    const int revertWidth = meta.hasTemplateValue ? 92 : 0;
    const int controlWidth = std::max(60, width - (revertWidth > 0 ? revertWidth + 10 : 0));
    SDL_Rect controlRect{x, controlY, controlWidth, controlHeight};

    std::string componentType;
    size_t index = 0;
    SplitComponentKey(activeComponentKey_, componentType, index);

    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        auto childIt = target->find(meta.path[i]);
        if (childIt == target->end() || !childIt->is_object()) {
            target = nullptr;
            break;
        }
        target = &(*childIt);
    }
    nlohmann::json currentValue;
    bool hasActualValue = false;
    if (target) {
        auto leafIt = target->find(meta.path.back());
        if (leafIt != target->end()) {
            currentValue = *leafIt;
            hasActualValue = true;
        }
    }
    const nlohmann::json* templateDefaults = GetTemplateDefaults(componentType);
    const nlohmann::json* templateValue = ResolveTemplateValue(templateDefaults, meta.path);
    const nlohmann::json* displayValue = hasActualValue ? &currentValue : templateValue;

    SDL_SetRenderDrawColor(renderer_, 40, 44, 52, 255);
    SDL_RenderFillRect(renderer_, &controlRect);
    SDL_SetRenderDrawColor(renderer_, meta.overridesTemplate ? kAccent.r : 60,
                           meta.overridesTemplate ? kAccent.g : 60,
                           meta.overridesTemplate ? kAccent.b : 60,
                           255);
    SDL_RenderDrawRect(renderer_, &controlRect);

    const SDL_Color valueColor = meta.overridesTemplate ? kTextColor : kTemplateValueColor;

    switch (meta.kind) {
        case FieldKind::kBoolean: {
            state.boolValue = (displayValue && displayValue->is_boolean()) ? displayValue->get<bool>() : false;
            SDL_Rect toggleRect{controlRect.x + controlRect.w - 70, controlRect.y + 6, 60, controlHeight - 12};
            SDL_SetRenderDrawColor(renderer_, state.boolValue ? 90 : 70, state.boolValue ? 180 : 70, 90, 255);
            SDL_RenderFillRect(renderer_, &toggleRect);
            SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
            SDL_RenderDrawRect(renderer_, &toggleRect);
            SDL_Rect knob{state.boolValue ? toggleRect.x + toggleRect.w - 24 : toggleRect.x + 4, toggleRect.y + 4, 20, toggleRect.h - 8};
            SDL_SetRenderDrawColor(renderer_, 245, 245, 245, 255);
            SDL_RenderFillRect(renderer_, &knob);
            boolToggleRects_[meta.id] = toggleRect;
            break;
        }
        case FieldKind::kEnum: {
            std::string textValue = (displayValue && displayValue->is_string()) ? displayValue->get<std::string>() : "";
            SDL_Rect textRect{controlRect.x + 8, controlRect.y + 6, controlRect.w - 40, controlHeight - 12};
            SDL_Rect buttonRect{controlRect.x + controlRect.w - 28, controlRect.y + 6, 22, controlHeight - 12};
            SDL_SetRenderDrawColor(renderer_, 24, 26, 32, 255);
            SDL_RenderFillRect(renderer_, &textRect);
            DrawText(textValue.empty() ? "<none>" : textValue, textRect.x + 4, textRect.y + 6, valueColor);
            SDL_SetRenderDrawColor(renderer_, 60, 64, 72, 255);
            SDL_RenderFillRect(renderer_, &buttonRect);
            SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
            SDL_RenderDrawRect(renderer_, &buttonRect);
            DrawText("⋁", buttonRect.x + 4, buttonRect.y + 4, kTextColor);
            state.primary.rect = textRect;
            enumButtonRects_[meta.id] = buttonRect;
            break;
        }
        case FieldKind::kVector2: {
            std::string xValue = "0";
            std::string yValue = "0";
            if (displayValue && displayValue->is_array() && displayValue->size() == 2) {
                xValue = FormatFloat((*displayValue)[0].get<double>());
                yValue = FormatFloat((*displayValue)[1].get<double>());
            }
            if (!state.primary.focused && focusedFieldId_ != meta.id + "|x") {
                state.primary.SetValue(xValue);
                state.primaryInitialized = true;
            }
            if (!state.secondary.focused && focusedFieldId_ != meta.id + "|y") {
                state.secondary.SetValue(yValue);
                state.secondaryInitialized = true;
            }
            const int halfWidth = (controlRect.w - 12) / 2;
            SDL_Rect leftRect{controlRect.x + 6, controlRect.y + 6, halfWidth, controlHeight - 12};
            SDL_Rect rightRect{controlRect.x + 6 + halfWidth + 6, controlRect.y + 6, halfWidth, controlHeight - 12};
            SDL_SetRenderDrawColor(renderer_, 24, 26, 32, 255);
            SDL_RenderFillRect(renderer_, &leftRect);
            SDL_RenderFillRect(renderer_, &rightRect);
            DrawText(state.primary.value, leftRect.x + 6, leftRect.y + 6, valueColor);
            DrawText(state.secondary.value, rightRect.x + 6, rightRect.y + 6, valueColor);
            state.primary.rect = leftRect;
            state.secondary.rect = rightRect;
            fieldRects_[meta.id + "|x"] = leftRect;
            fieldRects_[meta.id + "|y"] = rightRect;
            RenderCursor(state.primary);
            RenderCursor(state.secondary);
            break;
        }
        case FieldKind::kStringList: {
            std::string listValue;
            if (displayValue && displayValue->is_array()) {
                for (size_t i = 0; i < displayValue->size(); ++i) {
                    if (i > 0) {
                        listValue += ", ";
                    }
                    listValue += (*displayValue)[i].get<std::string>();
                }
            }
            if (!state.primary.focused || focusedFieldId_ != meta.id) {
                state.primary.SetValue(listValue);
                state.primaryInitialized = true;
            }
            SDL_Rect textRect{controlRect.x + 8, controlRect.y + 6, controlRect.w - 16, controlHeight - 12};
            SDL_SetRenderDrawColor(renderer_, 24, 26, 32, 255);
            SDL_RenderFillRect(renderer_, &textRect);
            DrawText(state.primary.value.empty() ? "<empty>" : state.primary.value, textRect.x + 6, textRect.y + 6, valueColor);
            state.primary.rect = textRect;
            fieldRects_[meta.id] = textRect;
            RenderCursor(state.primary);
            break;
        }
        case FieldKind::kNumber:
        case FieldKind::kString:
        default: {
            std::string valueText;
            if (displayValue) {
                if (displayValue->is_string()) {
                    valueText = displayValue->get<std::string>();
                } else if (displayValue->is_number_float()) {
                    valueText = FormatFloat(displayValue->get<double>());
                } else if (displayValue->is_number_integer()) {
                    valueText = std::to_string(displayValue->get<long long>());
                }
            }
            if (!state.primary.focused || focusedFieldId_ != meta.id) {
                state.primary.SetValue(valueText);
                state.primaryInitialized = true;
            }
            SDL_Rect textRect{controlRect.x + 8, controlRect.y + 6, controlRect.w - 16, controlHeight - 12};
            SDL_SetRenderDrawColor(renderer_, 24, 26, 32, 255);
            SDL_RenderFillRect(renderer_, &textRect);
            DrawText(state.primary.value, textRect.x + 6, textRect.y + 6, valueColor);
            state.primary.rect = textRect;
            fieldRects_[meta.id] = textRect;
            RenderCursor(state.primary);
            break;
        }
    }

    if (meta.hasTemplateValue) {
        SDL_Rect resetRect{controlRect.x + controlRect.w + 6, controlY, revertWidth ? revertWidth - 6 : 80, controlHeight};
        SDL_SetRenderDrawColor(renderer_, 50, 54, 60, 255);
        SDL_RenderFillRect(renderer_, &resetRect);
        SDL_SetRenderDrawColor(renderer_, 25, 27, 32, 255);
        SDL_RenderDrawRect(renderer_, &resetRect);
        DrawText("Revert", resetRect.x + 16, resetRect.y + 8, kTextColor);
        resetButtonRects_[meta.id] = resetRect;
    }
}

void ComponentEditorPanel::RenderSpecialComponentUI(const SDL_Rect& bounds) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    const std::string type = component->value("type", std::string{});
    const int buttonWidth = kSpecialButtonWidth;
    const int buttonHeight = kSpecialButtonHeight;
    const int usableWidth = bounds.w - 24;
    const int perRow = std::max(1, usableWidth / (buttonWidth + kSpecialButtonGap));
    int row = 0;
    int col = 0;

    auto drawButton = [&](const std::string& id, const std::string& label) {
        const int offsetX = bounds.x + 12 + col * (buttonWidth + kSpecialButtonGap);
        const int offsetY = bounds.y + bounds.h - (row + 1) * (buttonHeight + kSpecialButtonGap);
        SDL_Rect rect{offsetX, offsetY, buttonWidth, buttonHeight};
        SDL_SetRenderDrawColor(renderer_, 60, 64, 72, 255);
        SDL_RenderFillRect(renderer_, &rect);
        SDL_SetRenderDrawColor(renderer_, 30, 32, 36, 255);
        SDL_RenderDrawRect(renderer_, &rect);
        DrawText(label, rect.x + 12, rect.y + 7, kTextColor);
        specialActionButtons_.push_back({id, rect});
        ++col;
        if (col >= perRow) {
            col = 0;
            ++row;
        }
    };

    if (type == "SensorComponent") {
        drawButton("sensor:box", "Pick Box Zone");
    }
    if (type == "RailComponent") {
        drawButton("rail:path", "Edit Path");
    }
    if (type == "JointComponent") {
        drawButton("joint:anchors", "Pick Anchors");
    }
    if (type == "ObjectSpawnerComponent") {
        drawButton("spawner:locations", "Edit Spawn Points");
    }
}

void ComponentEditorPanel::RenderCursor(const TextField& field) {
    if (!field.focused || !font_ || field.rect.w <= 0) {
        return;
    }
    std::string prefix = field.value.substr(0, std::min(field.cursor, field.value.size()));
    int prefixWidth = 0;
    if (!prefix.empty()) {
        if (TTF_SizeUTF8(font_, prefix.c_str(), &prefixWidth, nullptr) != 0) {
            prefixWidth = 0;
        }
    }
    const int caretX = std::clamp(field.rect.x + 6 + prefixWidth, field.rect.x + 2, field.rect.x + field.rect.w - 2);
    SDL_Rect caret{caretX, field.rect.y + 4, 2, field.rect.h - 8};
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 230);
    SDL_RenderFillRect(renderer_, &caret);
}

void ComponentEditorPanel::ClearHeaderFocus(bool commitChanges) {
    if (commitChanges) {
        if (headerFocus_ == HeaderFocus::kObjectName) {
            ApplyObjectNameEdit();
        } else if (headerFocus_ == HeaderFocus::kTemplate) {
            ApplyTemplateEdit();
        }
    }
    if (headerFocus_ == HeaderFocus::kObjectName) {
        objectNameField_.Defocus();
    } else if (headerFocus_ == HeaderFocus::kTemplate) {
        templateField_.Defocus();
    }
    headerFocus_ = HeaderFocus::kNone;
    if (focusedFieldId_.empty()) {
        EndTextInput();
    }
}

void ComponentEditorPanel::ClearFocus() {
    if (!focusedFieldId_.empty()) {
        const auto decoded = DecodeFieldKey(focusedFieldId_);
        if (auto it = fieldStates_.find(decoded.metaId); it != fieldStates_.end()) {
            if (decoded.suffix == "y") {
                it->second.secondary.Defocus();
            } else {
                it->second.primary.Defocus();
            }
        }
    }
    focusedFieldId_.clear();
    if (headerFocus_ != HeaderFocus::kNone) {
        ClearHeaderFocus(true);
    } else {
        EndTextInput();
    }
}

bool ComponentEditorPanel::InstantiateTemplateComponent(const std::string& key) {
    auto templIt = templateRowData_.find(key);
    if (templIt == templateRowData_.end()) {
        return false;
    }
    auto* object = FindSelectedObject();
    if (!object) {
        return false;
    }
    auto& components = (*object)["components"];
    if (!components.is_array()) {
        components = nlohmann::json::array();
    }
    std::string type;
    size_t insertIndex = 0;
    if (!SplitComponentKey(key, type, insertIndex)) {
        return false;
    }
    nlohmann::json newComponent = templIt->second;
    if (!newComponent.contains("type")) {
        newComponent["type"] = type;
    }
    if (insertIndex > components.size()) {
        insertIndex = components.size();
    }
    components.insert(components.begin() + static_cast<std::ptrdiff_t>(insertIndex), newComponent);
    templateRowData_.erase(templIt);
    currentFields_.clear();
    fieldStates_.clear();
    MarkDirty();
    return true;
}

std::string ComponentEditorPanel::MakeFieldKey(const std::string& componentKey, const std::vector<std::string>& path) const {
    return componentKey + ":" + JoinPath(path);
}

std::string ComponentEditorPanel::PathToLabel(const std::vector<std::string>& path) const {
    return JoinPath(path);
}

void ComponentEditorPanel::ApplyFieldChange(const FieldMeta& meta, FieldState& state) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        auto& key = meta.path[i];
        if (!target->contains(key) || !(*target)[key].is_object()) {
            (*target)[key] = nlohmann::json::object();
        }
        target = &(*target)[key];
    }
    const std::string& leaf = meta.path.back();
    try {
        if (meta.kind == FieldKind::kNumber) {
            double parsed = std::stod(state.primary.value);
            (*target)[leaf] = parsed;
        } else {
            (*target)[leaf] = state.primary.value;
        }
        state.hasError = false;
        state.errorMessage.clear();
        MarkDirty();
    } catch (const std::exception&) {
        state.hasError = true;
        state.errorMessage = "Invalid value";
    }
}

void ComponentEditorPanel::ApplyVectorChange(const FieldMeta& meta, FieldState& state, bool primaryEdited) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        target = &(*target)[meta.path[i]];
    }
    const std::string& leaf = meta.path.back();
    try {
        double x = std::stod(state.primary.value);
        double y = std::stod(state.secondary.value);
        nlohmann::json vec = nlohmann::json::array();
        vec.push_back(x);
        vec.push_back(y);
        (*target)[leaf] = vec;
        state.hasError = false;
        state.errorMessage.clear();
        MarkDirty();
    } catch (const std::exception&) {
        state.hasError = true;
        state.errorMessage = "Invalid number";
    }
}

void ComponentEditorPanel::ApplyListChange(const FieldMeta& meta, FieldState& state) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        target = &(*target)[meta.path[i]];
    }
    const std::string& leaf = meta.path.back();

    nlohmann::json array = nlohmann::json::array();
    std::string input = state.primary.value;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = Trim(token);
        if (!token.empty()) {
            array.push_back(token);
        }
    }
    (*target)[leaf] = array;
    MarkDirty();
}

void ComponentEditorPanel::ApplyBoolChange(const FieldMeta& meta, FieldState& state) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        target = &(*target)[meta.path[i]];
    }
    (*target)[meta.path.back()] = state.boolValue;
    MarkDirty();
}

void ComponentEditorPanel::ApplyEnumSelection(const FieldMeta& meta, const std::string& value) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        target = &(*target)[meta.path[i]];
    }
    (*target)[meta.path.back()] = value;
    MarkDirty();
    CloseDropdown();
}

void ComponentEditorPanel::ApplyObjectNameEdit() {
    auto* object = FindSelectedObject();
    if (!object) {
        return;
    }
    const std::string currentName = object->value("name", activeObjectName_);
    std::string desired = Trim(objectNameField_.value);
    if (desired.empty()) {
        objectNameField_.SetValue(currentName);
        return;
    }
    if (desired == currentName) {
        return;
    }
    if (levelState_) {
        desired = levelState_->GenerateUniqueObjectName(desired);
    }
    (*object)["name"] = desired;
    activeObjectName_ = desired;
    selectionCache_ = {desired};
    if (levelState_) {
        levelState_->SetSelection(selectionCache_);
    }
    objectNameField_.SetValue(desired);
    MarkDirty();
}

void ComponentEditorPanel::ApplyTemplateEdit() {
    auto* object = FindSelectedObject();
    if (!object) {
        return;
    }
    const std::string currentTemplate = object->value("template", std::string{});
    std::string desired = Trim(templateField_.value);
    if (desired == currentTemplate) {
        return;
    }
    if (!desired.empty() && schemaService_ && !schemaService_->GetTemplate(desired)) {
        templateField_.SetValue(currentTemplate);
        return;
    }
    if (desired.empty()) {
        object->erase("template");
    } else {
        (*object)["template"] = desired;
    }
    templateField_.SetValue(desired);
    currentFields_.clear();
    fieldStates_.clear();
    MarkDirty();
}

void ComponentEditorPanel::ResetFieldToTemplate(const FieldMeta& meta) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    std::string componentType;
    size_t index = 0;
    if (!SplitComponentKey(activeComponentKey_, componentType, index)) {
        return;
    }
    auto* object = FindSelectedObject();
    if (!object) {
        return;
    }
    const std::string templateId = object->value("template", std::string{});
    if (templateId.empty() || !schemaService_) {
        return;
    }
    const auto* templateNode = schemaService_->GetTemplateComponentDefaults(templateId, componentType);
    if (!templateNode) {
        return;
    }
    const nlohmann::json* templateValue = templateNode;
    for (const auto& segment : meta.path) {
        if (!templateValue->is_object()) {
            templateValue = nullptr;
            break;
        }
        auto it = templateValue->find(segment);
        if (it == templateValue->end()) {
            templateValue = nullptr;
            break;
        }
        templateValue = &(*it);
    }
    nlohmann::json* target = component;
    for (size_t i = 0; i + 1 < meta.path.size(); ++i) {
        target = &(*target)[meta.path[i]];
    }
    if (!templateValue) {
        target->erase(meta.path.back());
    } else {
        (*target)[meta.path.back()] = *templateValue;
    }
    MarkDirty();
}

void ComponentEditorPanel::MarkDirty() {
    if (levelState_) {
        levelState_->MarkDirty();
    }
}

void ComponentEditorPanel::RemoveComponent(const std::string& componentKey) {
    auto* object = FindSelectedObject();
    if (!object || !object->contains("components") || !(*object)["components"].is_array()) {
        return;
    }
    auto& components = (*object)["components"];
    std::string type;
    size_t index = 0;
    if (!SplitComponentKey(componentKey, type, index) || index >= components.size()) {
        return;
    }
    components.erase(components.begin() + static_cast<std::ptrdiff_t>(index));
    activeComponentKey_.clear();
    currentFields_.clear();
    fieldStates_.clear();
    MarkDirty();
}

void ComponentEditorPanel::AddComponent(const std::string& type) {
    auto* object = FindSelectedObject();
    if (!object) {
        return;
    }
    auto& components = (*object)["components"];
    if (!components.is_array()) {
        components = nlohmann::json::array();
    }
    nlohmann::json newComponent;
    if (const auto* defaults = GetTemplateDefaults(type)) {
        newComponent = *defaults;
        if (!newComponent.contains("type")) {
            newComponent["type"] = type;
        }
    } else {
        newComponent["type"] = type;
    }
    components.push_back(newComponent);
    activeComponentKey_ = MakeComponentKey(type, components.size() - 1);
    currentFields_.clear();
    fieldStates_.clear();
    MarkDirty();
}

void ComponentEditorPanel::DuplicateComponent(const std::string& componentKey) {
    auto* object = FindSelectedObject();
    if (!object || !object->contains("components") || !(*object)["components"].is_array()) {
        return;
    }
    auto& components = (*object)["components"];
    std::string type;
    size_t index = 0;
    if (!SplitComponentKey(componentKey, type, index) || index >= components.size()) {
        return;
    }
    nlohmann::json clone = components.at(index);
    components.push_back(clone);
    activeComponentKey_ = MakeComponentKey(type, components.size() - 1);
    currentFields_.clear();
    fieldStates_.clear();
    MarkDirty();
}

void ComponentEditorPanel::OpenAddMenu(const SDL_Rect& anchor) {
    addMenu_.open = true;
    addMenu_.anchor = anchor;
    addMenu_.scroll = 0;
    if (schemaService_) {
        addMenu_.componentTypes = schemaService_->ListComponentTypes();
    }
}

void ComponentEditorPanel::CloseAddMenu() {
    addMenu_.open = false;
    addMenuEntryRects_.clear();
}

void ComponentEditorPanel::ToggleDropdown(const std::string& fieldId,
                                          const SDL_Rect& anchor,
                                          std::vector<std::string> options) {
    if (dropdown_.open && dropdown_.fieldId == fieldId) {
        dropdown_.open = false;
        return;
    }
    dropdown_.open = true;
    dropdown_.fieldId = fieldId;
    dropdown_.options = std::move(options);
    dropdown_.rect = {anchor.x, anchor.y + anchor.h + 4, anchor.w + 120, 150};
}

void ComponentEditorPanel::CloseDropdown() {
    dropdown_.open = false;
    dropdown_.options.clear();
    dropdown_.fieldId.clear();
}

void ComponentEditorPanel::TriggerMapInteraction(MapInteractionType type, const nlohmann::json& payload) {
    if (!callbacks_.onRequestMapInteraction) {
        return;
    }
    MapInteractionRequest request;
    request.type = type;
    request.payload = payload;
    request.onComplete = [this](const MapInteractionResult& result) { HandleMapResult(result); };
    request.onCancelled = []() {};
    callbacks_.onRequestMapInteraction(request);
}

void ComponentEditorPanel::HandleMapResult(const MapInteractionResult& result) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    const std::string type = component->value("type", std::string{});
    if (type == "SensorComponent") {
        HandleSensorResult(result.payload);
    } else if (type == "RailComponent") {
        HandleRailResult(result.payload);
    } else if (type == "JointComponent") {
        HandleJointResult(result.payload);
    } else if (type == "ObjectSpawnerComponent") {
        HandleSpawnerResult(result.payload);
    }
}

void ComponentEditorPanel::HandleSensorResult(const nlohmann::json& payload) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    (*component)["requireBoxZone"] = true;
    (*component)["boxZone"] = payload;
    MarkDirty();
    currentFields_.clear();
}

void ComponentEditorPanel::HandleRailResult(const nlohmann::json& payload) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    (*component)["path"] = payload;
    MarkDirty();
}

void ComponentEditorPanel::HandleJointResult(const nlohmann::json& payload) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    if (payload.contains("connectedBody")) {
        (*component)["connectedBody"] = payload["connectedBody"];
    }
    if (payload.contains("anchorA")) {
        (*component)["anchorA"] = payload["anchorA"];
    }
    if (payload.contains("anchorB")) {
        (*component)["anchorB"] = payload["anchorB"];
    }
    MarkDirty();
}

void ComponentEditorPanel::HandleSpawnerResult(const nlohmann::json& payload) {
    auto* component = FindActiveComponent();
    if (!component) {
        return;
    }
    (*component)["spawnLocations"] = payload;
    MarkDirty();
}

const nlohmann::json* ComponentEditorPanel::GetTemplateDefaults(const std::string& componentType) {
    if (!schemaService_) {
        return nullptr;
    }
    auto* object = FindSelectedObject();
    if (!object) {
        return nullptr;
    }
    const std::string templateId = object->value("template", std::string{});
    if (templateId.empty()) {
        return nullptr;
    }
    return schemaService_->GetTemplateComponentDefaults(templateId, componentType);
}

ComponentEditorPanel::FieldKind ComponentEditorPanel::FieldKindFromDescriptor(FieldType type) {
    switch (type) {
        case FieldType::kNumber:
            return FieldKind::kNumber;
        case FieldType::kBoolean:
            return FieldKind::kBoolean;
        case FieldType::kEnum:
            return FieldKind::kEnum;
        case FieldType::kVector2:
            return FieldKind::kVector2;
        default:
            return FieldKind::kString;
    }
}

void ComponentEditorPanel::DrawText(const std::string& text, int x, int y, SDL_Color color) {
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

}  // namespace level_editor


