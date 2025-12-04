#include "level_editor/EditorApp.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cmath>

#include <nlohmann/json.hpp>

namespace level_editor {

namespace {
const char* MapInteractionTypeName(MapInteractionType type) {
    switch (type) {
        case MapInteractionType::kSensorBox:
            return "SensorBox";
        case MapInteractionType::kRailPath:
            return "RailPath";
        case MapInteractionType::kJointAnchors:
            return "JointAnchors";
        case MapInteractionType::kSpawnerLocations:
            return "SpawnerLocations";
    }
    return "Unknown";
}

SchemaSources BuildDefaultSchemaSources() {
    SchemaSources sources;
    sources.templatesJson = "assets/objectData.json";
    sources.spritesJson = "assets/spriteData.json";
    sources.soundsJson = "assets/soundData.json";
    sources.additionalSearchRoots = {"src/components"};
    return sources;
}

FileServiceConfig BuildDefaultFileServiceConfig() {
    FileServiceConfig config;
    config.levelsDirectory = "assets/levels";
    return config;
}

void EnsureComponentsArray(nlohmann::json& object) {
    if (!object.contains("components") || !object["components"].is_array()) {
        object["components"] = nlohmann::json::array();
    }
}

nlohmann::json* FindComponent(nlohmann::json& object, const std::string& type) {
    EnsureComponentsArray(object);
    for (auto& component : object["components"]) {
        if (component.value("type", std::string{}) == type) {
            return &component;
        }
    }
    return nullptr;
}

nlohmann::json* EnsureComponent(nlohmann::json& object, const std::string& type) {
    if (auto* comp = FindComponent(object, type)) {
        return comp;
    }
    auto& components = object["components"];
    components.push_back(nlohmann::json::object());
    components.back()["type"] = type;
    return &components.back();
}

bool ReapplyBodyComponent(nlohmann::json& object, const nlohmann::json& templateBody) {
    auto* body = EnsureComponent(object, "BodyComponent");
    const double posX = body->value("posX", templateBody.value("posX", 0.0));
    const double posY = body->value("posY", templateBody.value("posY", 0.0));
    const double angle = body->value("angle", templateBody.value("angle", 0.0));

    nlohmann::json newBody = templateBody;
    newBody["type"] = "BodyComponent";
    newBody["posX"] = posX;
    newBody["posY"] = posY;
    newBody["angle"] = angle;

    if (*body != newBody) {
        *body = std::move(newBody);
        return true;
    }
    return false;
}

bool ReapplySpriteComponent(nlohmann::json& object, const nlohmann::json& templateSprite) {
    auto* sprite = EnsureComponent(object, "SpriteComponent");
    const double posX = sprite->value("posX", templateSprite.value("posX", 0.0));
    const double posY = sprite->value("posY", templateSprite.value("posY", 0.0));
    const double angle = sprite->value("angle", templateSprite.value("angle", 0.0));
    const double renderWidth = sprite->value("renderWidth", templateSprite.value("renderWidth", 0.0));
    const double renderHeight = sprite->value("renderHeight", templateSprite.value("renderHeight", 0.0));
    const int colorR = sprite->value("colorR", templateSprite.value("colorR", 255));
    const int colorG = sprite->value("colorG", templateSprite.value("colorG", 255));
    const int colorB = sprite->value("colorB", templateSprite.value("colorB", 255));
    const int alpha = sprite->value("alpha", templateSprite.value("alpha", 255));
    std::string spriteName = sprite->value("spriteName", std::string{});
    if (spriteName.empty()) {
        spriteName = templateSprite.value("spriteName", spriteName);
    }

    nlohmann::json newSprite = templateSprite;
    newSprite["type"] = "SpriteComponent";
    newSprite["posX"] = posX;
    newSprite["posY"] = posY;
    newSprite["angle"] = angle;
    newSprite["renderWidth"] = renderWidth;
    newSprite["renderHeight"] = renderHeight;
    newSprite["colorR"] = colorR;
    newSprite["colorG"] = colorG;
    newSprite["colorB"] = colorB;
    newSprite["alpha"] = alpha;
    if (!spriteName.empty()) {
        newSprite["spriteName"] = spriteName;
    }

    if (*sprite != newSprite) {
        *sprite = std::move(newSprite);
        return true;
    }
    return false;
}

bool ApplyTemplateToObject(nlohmann::json& object, const SchemaService& schemaService, const std::string& templateId) {
    bool changed = false;
    if (const auto* bodyDefaults = schemaService.GetTemplateComponentDefaults(templateId, "BodyComponent")) {
        changed |= ReapplyBodyComponent(object, *bodyDefaults);
    }
    if (const auto* spriteDefaults = schemaService.GetTemplateComponentDefaults(templateId, "SpriteComponent")) {
        changed |= ReapplySpriteComponent(object, *spriteDefaults);
    }
    return changed;
}

constexpr float kRotationHandleScreenOffsetPx = 24.0f;
constexpr float kRotationHandleScreenRadiusPx = 14.0f;
constexpr float kRotationHandleHitPaddingPx = 10.0f;
constexpr float kResizeHandleHitRadiusPx = 11.0f;
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

std::pair<float, float> RotateLocalToWorld(const BoundsInfo& bounds, float localX, float localY) {
    const float rad = bounds.angleDegrees * kDegToRad;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const float worldX = bounds.centerX + localX * cosA - localY * sinA;
    const float worldY = bounds.centerY + localX * sinA + localY * cosA;
    return {worldX, worldY};
}

struct ResizeHandleDescriptor {
    ResizeTool::Handle handle;
    int dirX;
    int dirY;
};

const std::array<ResizeHandleDescriptor, 8> kResizeBoxHandles = {
    ResizeHandleDescriptor{ResizeTool::Handle::kNorth, 0, -1},
    ResizeHandleDescriptor{ResizeTool::Handle::kSouth, 0, 1},
    ResizeHandleDescriptor{ResizeTool::Handle::kEast, 1, 0},
    ResizeHandleDescriptor{ResizeTool::Handle::kWest, -1, 0},
    ResizeHandleDescriptor{ResizeTool::Handle::kNorthEast, 1, -1},
    ResizeHandleDescriptor{ResizeTool::Handle::kNorthWest, -1, -1},
    ResizeHandleDescriptor{ResizeTool::Handle::kSouthEast, 1, 1},
    ResizeHandleDescriptor{ResizeTool::Handle::kSouthWest, -1, 1},
};

const std::array<ResizeHandleDescriptor, 4> kResizeCircleHandles = {
    ResizeHandleDescriptor{ResizeTool::Handle::kNorth, 0, -1},
    ResizeHandleDescriptor{ResizeTool::Handle::kSouth, 0, 1},
    ResizeHandleDescriptor{ResizeTool::Handle::kEast, 1, 0},
    ResizeHandleDescriptor{ResizeTool::Handle::kWest, -1, 0},
};

std::pair<float, float> ResizeHandleLocalPosition(const BoundsInfo& bounds, const ResizeHandleDescriptor& desc) {
    if (bounds.shape == BoundsShape::kCircle) {
        float nx = static_cast<float>(desc.dirX);
        float ny = static_cast<float>(desc.dirY);
        const float length = std::max(0.0001f, std::sqrt(nx * nx + ny * ny));
        nx /= length;
        ny /= length;
        return {nx * bounds.radius, ny * bounds.radius};
    }
    const float x = bounds.width * 0.5f * static_cast<float>(desc.dirX);
    const float y = bounds.height * 0.5f * static_cast<float>(desc.dirY);
    return {x, y};
}

constexpr int kToolMenuWidth = 220;
constexpr int kToolMenuRowHeight = 28;
constexpr int kToolMenuPadding = 10;

void DrawMenuText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    if (!renderer || !font || text.empty()) {
        return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) {
        return;
    }
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}

}  // namespace

EditorApp::EditorApp()
    : schemaSources_(BuildDefaultSchemaSources()),
      schemaService_(SchemaService(SchemaService::LoadAll(schemaSources_))),
      fileService_(&schemaService_, BuildDefaultFileServiceConfig()) {}

int EditorApp::Run() {
    if (!Initialize()) {
        return -1;
    }

    running_ = true;
    uint32_t lastTicks = SDL_GetTicks();

    while (running_) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ProcessEvent(event, running_);
        }

        const uint32_t currentTicks = SDL_GetTicks();
        const float deltaSeconds = static_cast<float>(currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        viewport_.Update(deltaSeconds);

        SDL_SetRenderDrawColor(renderer_, 15, 15, 18, 255);
        SDL_RenderClear(renderer_);

        SDL_RenderSetViewport(renderer_, nullptr);
        toolbar_.Render(toolbarWidth_, windowHeight_);

        const int viewportWidth = std::max(1, windowWidth_ - toolbarWidth_ - inspectorWidth_);
        SDL_Rect viewportRect{toolbarWidth_, 0, viewportWidth, windowHeight_};
        UpdateComponentPanelRect(viewportWidth);
        viewport_.OnViewportResized(viewportRect.w, viewportRect.h);
        SDL_RenderSetViewport(renderer_, &viewportRect);
        RenderContext context{viewportRect.w, viewportRect.h, renderer_, toolbarWidth_, 0};
        viewport_.Render(context);
        SDL_RenderSetViewport(renderer_, nullptr);
        componentPanel_.Render(componentPanelRect_);

        if (templateDragActive_) {
            RenderTemplateDragPreview();
        }

        if (toolMenuOpen_) {
            SDL_RenderSetViewport(renderer_, nullptr);
            RenderToolMenu();
        }

        if (fileDialog_.IsOpen()) {
            SDL_RenderSetViewport(renderer_, nullptr);
            fileDialog_.Render(windowWidth_, windowHeight_);
        }

        if (templateEditor_.IsOpen()) {
            SDL_RenderSetViewport(renderer_, nullptr);
            templateEditor_.Render(windowWidth_, windowHeight_);
        }

        SDL_RenderPresent(renderer_);
    }

    Shutdown();
    return 0;
}

bool EditorApp::Initialize() {
    if (!InitializeSDL()) {
        return false;
    }

    selectionTool_ = std::make_shared<SelectionTool>();
    placementTool_ = std::make_shared<PlacementTool>();
    placementTool_->SetLevelState(&levelState_);
    placementTool_->SetSchemaService(&schemaService_);
    moveRotateTool_ = std::make_shared<MoveRotateTool>();
    moveRotateTool_->SetLevelState(&levelState_);
    moveRotateTool_->SetSchemaService(&schemaService_);
    moveRotateTool_->SetAssetCache(&assetCache_);
    resizeTool_ = std::make_shared<ResizeTool>();
    resizeTool_->SetLevelState(&levelState_);
    resizeTool_->SetSchemaService(&schemaService_);
    resizeTool_->SetAssetCache(&assetCache_);
    componentEditTool_ = std::make_shared<ComponentEditTool>();

    assetCache_.Initialize(renderer_, "assets/spriteData.json", "assets/textures");
    viewport_.SetLevelState(&levelState_);
    viewport_.SetAssetCache(&assetCache_);
    viewport_.SetSchemaService(&schemaService_);
    toolController_.SetViewport(&viewport_);
    toolController_.RegisterTool(ToolId::kSelection, selectionTool_);
    toolController_.RegisterTool(ToolId::kPlacement, placementTool_);
    toolController_.RegisterTool(ToolId::kMoveRotate, moveRotateTool_);
    toolController_.RegisterTool(ToolId::kResize, resizeTool_);
    toolController_.RegisterTool(ToolId::kComponentEdit, componentEditTool_);
    fileDialog_.Initialize(renderer_, uiFont_);
    templateEditor_.Initialize(renderer_, uiFont_);
    toolbar_.Initialize(renderer_, uiFont_, &schemaService_, &assetCache_);
    componentPanel_.Initialize(renderer_, uiFont_, &levelState_, &schemaService_, &assetCache_);
    ToolbarPanel::Callbacks callbacks;
    callbacks.onNew = [this]() { HandleNewLevel(); };
    callbacks.onLoad = [this]() {
        OpenLoadDialog();
    };
    callbacks.onSave = [this]() { HandleSaveLevel(currentLevelPath_); };
    callbacks.onTemplateSelected = [this](const std::string& templateId) {
        if (placementTool_) {
            placementTool_->SetActiveTemplate(templateId);
        }
    };
    callbacks.onApplyTemplate = [this](const std::string& templateId) {
        ApplyTemplateToObjects(templateId);
    };
    callbacks.onEditTemplate = [this](const std::string& templateId) {
        OpenTemplateEditor(templateId);
    };
    callbacks.onBeginDrag = [this](const std::string& templateId, int screenX, int screenY) {
        BeginTemplateDrag(templateId, screenX, screenY);
    };
    toolbar_.SetCallbacks(callbacks);
    ComponentEditorPanel::Callbacks panelCallbacks;
    panelCallbacks.onRequestMapInteraction = [this](const MapInteractionRequest& request) { BeginMapInteraction(request); };
    componentPanel_.SetCallbacks(panelCallbacks);
    const int initialViewportWidth = std::max(1, windowWidth_ - toolbarWidth_ - inspectorWidth_);
    UpdateComponentPanelRect(initialViewportWidth);
    toolMenuEntries_ = {
        {"Move / Rotate (M)", ToolId::kMoveRotate, SDLK_m},
        {"Resize (R)", ToolId::kResize, SDLK_r},
    };
    ActivateTool(ToolId::kSelection);
    LoadStartupLevel();
    return true;
}

void EditorApp::Shutdown() {
    running_ = false;
    ShutdownSDL();
}

bool EditorApp::HandleToolbarEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.x < toolbarWidth_) {
                return toolbar_.HandleEvent(event, toolbarWidth_, windowHeight_);
            }
            break;
        case SDL_MOUSEMOTION:
            if (event.motion.x < toolbarWidth_) {
                return toolbar_.HandleEvent(event, toolbarWidth_, windowHeight_);
            }
            break;
        case SDL_MOUSEWHEEL: {
            int mouseX = 0;
            int mouseY = 0;
            SDL_GetMouseState(&mouseX, &mouseY);
            if (mouseX < toolbarWidth_) {
                return toolbar_.HandleEvent(event, toolbarWidth_, windowHeight_);
            }
            break;
        }
        default:
            break;
    }
    return false;
}

bool EditorApp::HandleComponentPanelEvent(const SDL_Event& event) {
    if (componentPanelRect_.w <= 0 || componentPanelRect_.h <= 0) {
        return false;
    }
    return componentPanel_.HandleEvent(event, componentPanelRect_);
}

void EditorApp::UpdateComponentPanelRect(int viewportWidth) {
    int fallbackX = std::max(toolbarWidth_, windowWidth_ - inspectorWidth_);
    int panelX = std::max(toolbarWidth_ + viewportWidth, fallbackX);
    int panelWidth = std::max(0, windowWidth_ - panelX);
    if (panelWidth == 0 && windowWidth_ > toolbarWidth_) {
        panelWidth = std::min(inspectorWidth_, windowWidth_ - toolbarWidth_);
        panelX = windowWidth_ - panelWidth;
    }
    componentPanelRect_ = {panelX, 0, panelWidth, windowHeight_};
}

bool EditorApp::InitializeSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "[LevelEditor] SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    const int imgFlags = IMG_INIT_PNG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
        std::cerr << "[LevelEditor] IMG_Init failed: " << IMG_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    if (TTF_Init() != 0) {
        std::cerr << "[LevelEditor] TTF_Init failed: " << TTF_GetError() << "\n";
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    window_ = SDL_CreateWindow("Level Editor",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               windowWidth_,
                               windowHeight_,
                               SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "[LevelEditor] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "[LevelEditor] SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    uiFont_ = TTF_OpenFont("assets/fonts/ARIAL.TTF", 16);
    if (!uiFont_) {
        std::cerr << "[LevelEditor] Failed to load UI font: " << TTF_GetError() << "\n";
        return false;
    }

    viewport_.OnViewportResized(std::max(1, windowWidth_ - toolbarWidth_ - inspectorWidth_), windowHeight_);
    return true;
}

void EditorApp::ShutdownSDL() {
    if (uiFont_) {
        TTF_CloseFont(uiFont_);
        uiFont_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

static MouseButton TranslateMouseButton(uint8_t button) {
    switch (button) {
        case SDL_BUTTON_LEFT:
            return MouseButton::kLeft;
        case SDL_BUTTON_RIGHT:
            return MouseButton::kRight;
        case SDL_BUTTON_MIDDLE:
            return MouseButton::kMiddle;
        default:
            return MouseButton::kNone;
    }
}

static Key TranslateKey(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_W:
            return Key::kW;
        case SDL_SCANCODE_A:
            return Key::kA;
        case SDL_SCANCODE_S:
            return Key::kS;
        case SDL_SCANCODE_D:
            return Key::kD;
        case SDL_SCANCODE_ESCAPE:
            return Key::kEscape;
        default:
            return Key::kUnknown;
    }
}

void EditorApp::ProcessEvent(const SDL_Event& event, bool& running) {
    if (event.type == SDL_QUIT) {
        running = false;
        return;
    }
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        windowWidth_ = event.window.data1;
        windowHeight_ = event.window.data2;
        const int viewportWidth = std::max(1, windowWidth_ - toolbarWidth_ - inspectorWidth_);
        UpdateComponentPanelRect(viewportWidth);
        viewport_.OnViewportResized(viewportWidth, windowHeight_);
        return;
    }

    if (templateEditor_.IsOpen()) {
        templateEditor_.HandleEvent(event);
        return;
    }

    if (fileDialog_.IsOpen()) {
        if (fileDialog_.HandleEvent(event)) {
            return;
        }
        // Modal dialog consumes interaction even when it doesn't handle the event.
        return;
    }

    if (HandleComponentPanelEvent(event)) {
        return;
    }

    if (toolMenuOpen_) {
        if (HandleToolMenuEvent(event)) {
            return;
        }
    }

    const bool mapInteractionActive = componentEditTool_ && componentEditTool_->HasActiveInteraction();

    auto isToolbarMouseEvent = [&](const SDL_Event& evt) {
        switch (evt.type) {
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                return evt.button.x < toolbarWidth_;
            case SDL_MOUSEWHEEL: {
                int mx = 0;
                int my = 0;
                SDL_GetMouseState(&mx, &my);
                return mx < toolbarWidth_;
            }
            default:
                return false;
        }
    };

    if (mapInteractionActive && isToolbarMouseEvent(event)) {
        std::cout << "[EditorApp] Map interaction active; ignoring toolbar event type " << event.type << "\n";
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_p) {
            if (mapInteractionActive && componentEditTool_) {
                componentEditTool_->CancelInteraction();
            }
            ActivateTool(ToolId::kPlacement);
            return;
        }
        if (event.key.keysym.sym == SDLK_m) {
            if (mapInteractionActive && componentEditTool_) {
                componentEditTool_->CancelInteraction();
            }
            ActivateTool(ToolId::kMoveRotate);
            return;
        }
        if (event.key.keysym.sym == SDLK_r) {
            if (mapInteractionActive && componentEditTool_) {
                componentEditTool_->CancelInteraction();
            }
            ActivateTool(ToolId::kResize);
            return;
        }
        if (event.key.keysym.sym == SDLK_ESCAPE) {
            if (componentEditTool_ && componentEditTool_->HasActiveInteraction()) {
                std::cout << "[EditorApp] ESC pressed; cancelling active map interaction\n";
                componentEditTool_->CancelInteraction();
                return;
            }
            ActivateTool(ToolId::kSelection);
            return;
        }
    }

    const bool isMouseDown = event.type == SDL_MOUSEBUTTONDOWN;
    const bool isMouseUp = event.type == SDL_MOUSEBUTTONUP;
    const bool isMouseMotion = event.type == SDL_MOUSEMOTION;
    int mouseX = 0;
    int mouseY = 0;
    if (isMouseDown || isMouseUp) {
        mouseX = event.button.x;
        mouseY = event.button.y;
    } else if (isMouseMotion) {
        mouseX = event.motion.x;
        mouseY = event.motion.y;
    }

    const int viewportRight = componentPanelRect_.x;
    const bool insideViewport = mouseX >= toolbarWidth_ && mouseX < viewportRight && mouseY >= 0 && mouseY < windowHeight_;
    if (!mapInteractionActive && isMouseDown && insideViewport) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            const bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
            if (shiftHeld && placementTool_) {
                const auto world = viewport_.ScreenToWorld(static_cast<float>(mouseX), static_cast<float>(mouseY));
                placementTool_->PlaceObjectAt(world.first, world.second);
                CloseToolMenu();
                return;
            }
            const auto activeTool = toolController_.GetActiveTool();
            if (activeTool == ToolId::kMoveRotate || activeTool == ToolId::kResize) {
                const auto clickedName = PickObjectAtScreen(mouseX, mouseY);
                const auto& selection = levelState_.GetSelection();
                const bool clickedCurrent = !clickedName.empty() && !selection.empty() && selection.front() == clickedName;
                const bool nearHandle = IsPointOnActiveHandle(mouseX, mouseY);
                if ((clickedName.empty() || !clickedCurrent) && !nearHandle) {
                    HandleSelectionClick(mouseX, mouseY);
                    ActivateTool(ToolId::kSelection);
                    CloseToolMenu();
                    return;
                }
            }
            if (toolController_.GetActiveTool() == ToolId::kSelection) {
                HandleSelectionClick(mouseX, mouseY);
                CloseToolMenu();
                return;
            }
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            if (HandleViewportContextMenuRequest(mouseX, mouseY)) {
                return;
            }
        }
    }

    if (templateDragActive_) {
        if (event.type == SDL_MOUSEMOTION) {
            UpdateTemplateDrag(event.motion.x, event.motion.y);
            return;
        }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            const bool dropInViewport = event.button.x >= toolbarWidth_ && event.button.x < viewportRight &&
                                        event.button.y >= 0 && event.button.y < windowHeight_;
            EndTemplateDrag(dropInViewport, event.button.x, event.button.y);
            return;
        }
    }

    if (HandleToolbarEvent(event)) {
        return;
    }

    InputEvent inputEvent;
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            inputEvent.type = event.type == SDL_MOUSEBUTTONDOWN ? InputEvent::Type::kMouseDown : InputEvent::Type::kMouseUp;
            inputEvent.mouseButton = TranslateMouseButton(event.button.button);
            inputEvent.x = event.button.x;
            inputEvent.y = event.button.y;
            break;
        case SDL_MOUSEMOTION:
            inputEvent.type = InputEvent::Type::kMouseMove;
            inputEvent.x = event.motion.x;
            inputEvent.y = event.motion.y;
            break;
        case SDL_MOUSEWHEEL:
            inputEvent.type = InputEvent::Type::kScroll;
            SDL_GetMouseState(&inputEvent.x, &inputEvent.y);
            inputEvent.delta = event.wheel.y;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            inputEvent.type = event.type == SDL_KEYDOWN ? InputEvent::Type::kKeyDown : InputEvent::Type::kKeyUp;
            inputEvent.key = TranslateKey(event.key.keysym.scancode);
            break;
        default:
            return;
    }

    viewport_.HandleInput(inputEvent);
}

void EditorApp::LoadStartupLevel() {
    const std::string defaultLevel = "assets/levels/level1.json";
    if (!HandleLoadLevel(defaultLevel)) {
        HandleNewLevel();
    }
}

void EditorApp::HandleNewLevel() {
    levelState_.SetDocument(fileService_.NewLevel());
    currentLevelPath_.clear();
}

void EditorApp::OpenLoadDialog() {
    std::vector<std::string> files;
    namespace fs = std::filesystem;
    const fs::path levelsDir = "assets/levels";
    std::error_code ec;
    if (fs::exists(levelsDir, ec)) {
        for (const auto& entry : fs::directory_iterator(levelsDir, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    if (files.empty()) {
        std::cout << "[LevelEditor] No level files found in assets/levels.\n";
        return;
    }
    fileDialog_.OpenLoad(files, [this](const std::string& filename) {
        HandleLoadLevel((std::filesystem::path("assets/levels") / filename).string());
    });
}

bool EditorApp::HandleLoadLevel(const std::string& path) {
    auto result = fileService_.LoadLevel(path);
    if (result.success) {
        levelState_.SetDocument(std::move(result.document));
        currentLevelPath_ = path;
        levelState_.UpdateSourcePath(path);
        if (!result.issues.empty()) {
            std::cout << "[LevelEditor] Loaded with validation warnings.\n";
        }
        return true;
    }
    std::cout << "[LevelEditor] Failed to load " << path << ": " << result.message << "\n";
    return false;
}

bool EditorApp::HandleSaveLevel(const std::string& path) {
    std::string targetPath = path;
    if (targetPath.empty()) {
        targetPath = currentLevelPath_.empty() ? "assets/levels/level_editor_autosave.json" : currentLevelPath_;
    }
    if (fileService_.SaveLevel(targetPath, levelState_.GetDocument())) {
        currentLevelPath_ = targetPath;
        levelState_.UpdateSourcePath(targetPath);
        std::cout << "[LevelEditor] Saved level to " << targetPath << "\n";
        return true;
    }
    std::cout << "[LevelEditor] Failed to save level to " << targetPath << "\n";
    return false;
}

void EditorApp::OpenTemplateEditor(const std::string& templateId) {
    if (templateId.empty() || templateEditor_.IsOpen()) {
        return;
    }
    nlohmann::json templatesJson;
    if (!LoadTemplatesFile(templatesJson)) {
        std::cout << "[LevelEditor] Unable to open template file for editing.\n";
        return;
    }
    auto templatesIt = templatesJson.find("templates");
    if (templatesIt == templatesJson.end() || !templatesIt->is_object()) {
        std::cout << "[LevelEditor] Template file is missing a 'templates' object.\n";
        return;
    }
    auto entryIt = templatesIt->find(templateId);
    if (entryIt == templatesIt->end()) {
        std::cout << "[LevelEditor] Template " << templateId << " not found in objectData.json.\n";
        return;
    }
    templateEditor_.Open(templateId, *entryIt, [this](const TemplateEditResult& result) {
        HandleTemplateEditResult(result);
    });
}

void EditorApp::HandleTemplateEditResult(const TemplateEditResult& result) {
    nlohmann::json templatesJson;
    if (!LoadTemplatesFile(templatesJson)) {
        std::cout << "[LevelEditor] Failed to reload template file after edit.\n";
        return;
    }
    auto& templatesObject = templatesJson["templates"];
    if (!templatesObject.is_object()) {
        templatesObject = nlohmann::json::object();
    }
    templatesObject[result.templateId] = result.templateJson;
    if (!SaveTemplatesFile(templatesJson)) {
        std::cout << "[LevelEditor] Failed to save template file.\n";
        return;
    }

    schemaService_.Reload(schemaSources_);
    toolbar_.ReloadTemplates();
    viewport_.SetSchemaService(&schemaService_);
    if (placementTool_) {
        placementTool_->SetSchemaService(&schemaService_);
    }
    if (moveRotateTool_) {
        moveRotateTool_->SetSchemaService(&schemaService_);
    }
    if (resizeTool_) {
        resizeTool_->SetSchemaService(&schemaService_);
    }

    if (result.applyToExisting) {
        ApplyTemplateToObjects(result.templateId);
    }
    std::cout << "[LevelEditor] Template " << result.templateId << " updated.\n";
}

bool EditorApp::LoadTemplatesFile(nlohmann::json& out) const {
    std::ifstream input(schemaSources_.templatesJson);
    if (!input.is_open()) {
        return false;
    }
    try {
        input >> out;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

bool EditorApp::SaveTemplatesFile(const nlohmann::json& data) const {
    std::ofstream output(schemaSources_.templatesJson, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    try {
        output << data.dump(4);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

void EditorApp::ApplyTemplateToObjects(const std::string& templateId) {
    if (templateId.empty()) {
        return;
    }
    if (!schemaService_.GetTemplate(templateId)) {
        return;
    }

    auto& document = levelState_.GetDocument();
    auto& data = document.data;
    if (!data.contains("objects") || !data["objects"].is_array()) {
        return;
    }

    bool changed = false;
    for (auto& object : data["objects"]) {
        if (!object.is_object()) {
            continue;
        }
        const std::string objectTemplate = object.value("template", std::string{});
        if (objectTemplate != templateId) {
            continue;
        }
        if (ApplyTemplateToObject(object, schemaService_, templateId)) {
            changed = true;
        }
    }

    if (changed) {
        levelState_.MarkDirty();
        std::cout << "[LevelEditor] Reapplied template defaults for " << templateId << "\n";
    }
}

void EditorApp::ActivateTool(ToolId toolId) {
    toolController_.ActivateTool(toolId);
    CloseToolMenu();
}

void EditorApp::BeginMapInteraction(const MapInteractionRequest& request) {
    if (!componentEditTool_) {
        if (request.onCancelled) {
            request.onCancelled();
        }
        return;
    }
    std::cout << "[EditorApp] Begin map interaction: " << MapInteractionTypeName(request.type) << "\n";
    MapInteractionRequest wrapped = request;
    wrapped.onCancelled = [this, type = request.type, userHandler = request.onCancelled]() {
        if (userHandler) {
            userHandler();
        }
        OnMapInteractionFinished();
    };
    wrapped.onComplete = [onComplete = request.onComplete](const MapInteractionResult& result) {
        if (onComplete) {
            onComplete(result);
        }
    };
    componentEditTool_->BeginInteraction(wrapped, [this]() { OnMapInteractionFinished(); });
    if (!componentEditTool_->HasActiveInteraction()) {
        return;
    }
    previousToolBeforeMap_ = toolController_.GetActiveTool();
    toolController_.ActivateTool(ToolId::kComponentEdit);
}

void EditorApp::OnMapInteractionFinished() {
    const auto target = previousToolBeforeMap_ == ToolId::kNone ? ToolId::kSelection : previousToolBeforeMap_;
    toolController_.ActivateTool(target);
    previousToolBeforeMap_ = ToolId::kSelection;
}

void EditorApp::BeginTemplateDrag(const std::string& templateId, int screenX, int screenY) {
    templateDragActive_ = true;
    templateDragTemplateId_ = templateId;
    templateDragX_ = screenX;
    templateDragY_ = screenY;
    if (placementTool_) {
        placementTool_->SetActiveTemplate(templateId);
    }
}

void EditorApp::UpdateTemplateDrag(int screenX, int screenY) {
    if (!templateDragActive_) {
        return;
    }
    templateDragX_ = screenX;
    templateDragY_ = screenY;
}

void EditorApp::EndTemplateDrag(bool place, int screenX, int screenY) {
    if (!templateDragActive_) {
        return;
    }
    if (place && placementTool_) {
        const auto world = viewport_.ScreenToWorld(static_cast<float>(screenX), static_cast<float>(screenY));
        placementTool_->PlaceObjectAt(world.first, world.second);
    }
    templateDragActive_ = false;
    templateDragTemplateId_.clear();
}

void EditorApp::RenderTemplateDragPreview() {
    if (!templateDragActive_ || templateDragTemplateId_.empty()) {
        return;
    }

    const bool overViewport = templateDragX_ >= toolbarWidth_ && templateDragX_ < componentPanelRect_.x &&
                              templateDragY_ >= 0 && templateDragY_ < windowHeight_;
    if (!overViewport) {
        return;
    }

    const auto world = viewport_.ScreenToWorld(static_cast<float>(templateDragX_), static_cast<float>(templateDragY_));
    const auto screen = viewport_.WorldToScreen(world.first, world.second);

    const auto* templateBody = schemaService_.GetTemplateComponentDefaults(templateDragTemplateId_, "BodyComponent");
    const auto* templateSprite = schemaService_.GetTemplateComponentDefaults(templateDragTemplateId_, "SpriteComponent");
    auto readFixtureFloat = [&](const char* key, float fallback) -> float {
        if (templateBody && templateBody->contains("fixture")) {
            const auto& fixture = (*templateBody)["fixture"];
            if (fixture.contains(key) && fixture[key].is_number()) {
                return fixture[key].get<float>();
            }
        }
        return fallback;
    };
    auto readFixtureString = [&](const char* key, const std::string& fallback) -> std::string {
        if (templateBody && templateBody->contains("fixture")) {
            const auto& fixture = (*templateBody)["fixture"];
            if (fixture.contains(key) && fixture[key].is_string()) {
                return fixture[key].get<std::string>();
            }
        }
        return fallback;
    };
    auto readSpriteFloat = [&](const char* key, float fallback) -> float {
        if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_number()) {
            return (*templateSprite)[key].get<float>();
        }
        return fallback;
    };

    std::string spriteName;
    if (templateSprite && templateSprite->contains("spriteName") && (*templateSprite)["spriteName"].is_string()) {
        spriteName = (*templateSprite)["spriteName"].get<std::string>();
    }
    if (spriteName.empty()) {
        if (const auto* descriptor = schemaService_.GetTemplate(templateDragTemplateId_)) {
            spriteName = descriptor->spriteName.empty() ? templateDragTemplateId_ : descriptor->spriteName;
        } else {
            spriteName = templateDragTemplateId_;
        }
    }

    float width = 0.0f;
    float height = 0.0f;
    if (templateBody && templateBody->contains("fixture")) {
        const std::string shape = readFixtureString("shape", "box");
        if (shape == "circle") {
            const float radius = readFixtureFloat("radius", 32.0f);
            width = height = radius * 2.0f;
        } else {
            width = readFixtureFloat("width", width);
            height = readFixtureFloat("height", height);
        }
    }
    const float renderWidth = readSpriteFloat("renderWidth", 0.0f);
    const float renderHeight = readSpriteFloat("renderHeight", 0.0f);
    if (renderWidth > 0.0f) {
        width = renderWidth;
    }
    if (renderHeight > 0.0f) {
        height = renderHeight;
    }

    SDL_Texture* texture = nullptr;
    SDL_Rect src{0, 0, 0, 0};
    if (const auto* spriteInfo = assetCache_.GetSpriteInfo(spriteName); spriteInfo && !spriteInfo->frames.empty()) {
        const auto& frame = spriteInfo->frames[0];
        texture = assetCache_.GetTexture(frame.textureName);
        src = {frame.x, frame.y, frame.w, frame.h};
        if (width <= 0.0f) {
            width = static_cast<float>(frame.w);
        }
        if (height <= 0.0f) {
            height = static_cast<float>(frame.h);
        }
    }

    constexpr float kPreviewMinSize = 16.0f;
    if (width <= 0.0f) {
        width = 64.0f;
    }
    if (height <= 0.0f) {
        height = 64.0f;
    }

    const float zoom = viewport_.GetCamera().zoom;
    float screenWidth = std::max(kPreviewMinSize, width * zoom);
    float screenHeight = std::max(kPreviewMinSize, height * zoom);
    SDL_FRect dest{toolbarWidth_ + screen.first - screenWidth * 0.5f,
                   screen.second - screenHeight * 0.5f,
                   screenWidth,
                   screenHeight};

    if (texture && src.w > 0 && src.h > 0) {
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(texture, 160);
        SDL_RenderCopyF(renderer_, texture, &src, &dest);
    } else {
        SDL_SetRenderDrawColor(renderer_, 90, 180, 255, 160);
        SDL_RenderFillRectF(renderer_, &dest);
    }
}

bool EditorApp::HandleSelectionClick(int screenX, int screenY) {
    const auto name = PickObjectAtScreen(screenX, screenY);
    if (name.empty()) {
        levelState_.SetSelection({});
    } else {
        levelState_.SetSelection({name});
    }
    return true;
}

bool EditorApp::HandleViewportContextMenuRequest(int screenX, int screenY) {
    if (levelState_.GetSelection().empty()) {
        return false;
    }
    if (!IsPointInsideViewport(screenX, screenY)) {
        return false;
    }
    const auto name = PickObjectAtScreen(screenX, screenY);
    if (name.empty()) {
        return false;
    }
    const auto& selection = levelState_.GetSelection();
    if (selection.empty() || selection.front() != name) {
        return false;
    }
    ShowToolMenu(screenX, screenY);
    return true;
}

void EditorApp::ShowToolMenu(int screenX, int screenY) {
    if (toolMenuEntries_.empty()) {
        return;
    }
    toolMenuOpen_ = true;
    toolMenuHoverIndex_ = -1;
    const int height = kToolMenuPadding * 2 + static_cast<int>(toolMenuEntries_.size()) * kToolMenuRowHeight;
    int clampedX = screenX;
    int clampedY = screenY;
    const int maxX = componentPanelRect_.x;
    if (clampedX + kToolMenuWidth > maxX) {
        clampedX = maxX - kToolMenuWidth;
    }
    if (clampedY + height > windowHeight_) {
        clampedY = windowHeight_ - height;
    }
    clampedX = std::max(toolbarWidth_, clampedX);
    clampedY = std::max(0, clampedY);
    toolMenuRect_ = {clampedX, clampedY, kToolMenuWidth, height};
}

bool EditorApp::HandleToolMenuEvent(const SDL_Event& event) {
    if (!toolMenuOpen_) {
        return false;
    }
    switch (event.type) {
        case SDL_MOUSEMOTION:
            toolMenuHoverIndex_ = ToolMenuEntryIndexAt(event.motion.x, event.motion.y);
            return true;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const int index = ToolMenuEntryIndexAt(event.button.x, event.button.y);
                if (index >= 0) {
                    ActivateTool(toolMenuEntries_[index].toolId);
                }
                CloseToolMenu();
                return true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                CloseToolMenu();
                return true;
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                CloseToolMenu();
                return true;
            }
            for (const auto& entry : toolMenuEntries_) {
                if (entry.hotkey == event.key.keysym.sym) {
                    ActivateTool(entry.toolId);
                    return true;
                }
            }
            break;
        default:
            break;
    }
    return false;
}

void EditorApp::RenderToolMenu() {
    if (!toolMenuOpen_ || toolMenuEntries_.empty() || !renderer_ || !uiFont_) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, 35, 37, 42, 240);
    SDL_RenderFillRect(renderer_, &toolMenuRect_);
    SDL_SetRenderDrawColor(renderer_, 70, 75, 85, 255);
    SDL_RenderDrawRect(renderer_, &toolMenuRect_);

    int entryY = toolMenuRect_.y + kToolMenuPadding;
    for (size_t i = 0; i < toolMenuEntries_.size(); ++i) {
        SDL_Rect rowRect{toolMenuRect_.x + kToolMenuPadding,
                         entryY,
                         toolMenuRect_.w - kToolMenuPadding * 2,
                         kToolMenuRowHeight};
        if (static_cast<int>(i) == toolMenuHoverIndex_) {
            SDL_SetRenderDrawColor(renderer_, 80, 130, 200, 255);
            SDL_RenderFillRect(renderer_, &rowRect);
        }
        SDL_Color textColor{230, 230, 235, 255};
        DrawMenuText(renderer_, uiFont_, toolMenuEntries_[i].label, rowRect.x + 6, rowRect.y + 6, textColor);
        entryY += kToolMenuRowHeight;
    }
}

void EditorApp::CloseToolMenu() {
    toolMenuOpen_ = false;
    toolMenuHoverIndex_ = -1;
}

std::string EditorApp::PickObjectAtScreen(int screenX, int screenY) const {
    auto* mutableState = const_cast<LevelState*>(&levelState_);
    auto* schema = &schemaService_;
    auto* assets = const_cast<AssetCache*>(&assetCache_);
    const auto world = viewport_.ScreenToWorld(static_cast<float>(screenX), static_cast<float>(screenY));
    return tool_utils::PickObject(mutableState, world.first, world.second, schema, assets);
}

bool EditorApp::IsPointInsideViewport(int screenX, int screenY) const {
    return screenX >= toolbarWidth_ && screenX < componentPanelRect_.x && screenY >= 0 && screenY < windowHeight_;
}

int EditorApp::ToolMenuEntryIndexAt(int screenX, int screenY) const {
    if (!toolMenuOpen_) {
        return -1;
    }
    if (screenX < toolMenuRect_.x || screenX >= toolMenuRect_.x + toolMenuRect_.w || screenY < toolMenuRect_.y ||
        screenY >= toolMenuRect_.y + toolMenuRect_.h) {
        return -1;
    }
    const int relativeY = screenY - (toolMenuRect_.y + kToolMenuPadding);
    if (relativeY < 0) {
        return -1;
    }
    const int index = relativeY / kToolMenuRowHeight;
    if (index < 0 || index >= static_cast<int>(toolMenuEntries_.size())) {
        return -1;
    }
    return index;
}

const nlohmann::json* EditorApp::GetSelectedObject() const {
    const auto& selection = levelState_.GetSelection();
    if (selection.empty()) {
        return nullptr;
    }
    const auto& doc = levelState_.GetDocument().data;
    const auto objectsIt = doc.find("objects");
    if (objectsIt == doc.end() || !objectsIt->is_array()) {
        return nullptr;
    }
    for (const auto& object : *objectsIt) {
        if (object.value("name", std::string{}) == selection.front()) {
            return &object;
        }
    }
    return nullptr;
}

bool EditorApp::IsPointOnActiveHandle(int screenX, int screenY) const {
    const auto activeTool = toolController_.GetActiveTool();
    if (activeTool != ToolId::kMoveRotate && activeTool != ToolId::kResize) {
        return false;
    }
    const auto* object = GetSelectedObject();
    if (!object) {
        return false;
    }
    BoundsInfo bounds;
    if (!tool_utils::ComputeBounds(*object, bounds, &schemaService_, const_cast<AssetCache*>(&assetCache_))) {
        return false;
    }
    if (activeTool == ToolId::kMoveRotate) {
        return IsPointNearMoveRotateHandle(bounds, screenX, screenY);
    }
    return IsPointNearResizeHandle(bounds, screenX, screenY);
}

bool EditorApp::IsPointNearMoveRotateHandle(const BoundsInfo& bounds, int screenX, int screenY) const {
    const float zoom = viewport_.GetCamera().zoom;
    const float offsetWorld = kRotationHandleScreenOffsetPx / std::max(zoom, 0.001f);
    const float extent = bounds.shape == BoundsShape::kCircle ? bounds.radius : bounds.height * 0.5f;
    const auto handleWorld = RotateLocalToWorld(bounds, 0.0f, -(extent + offsetWorld));
    const auto handleScreen = viewport_.WorldToScreen(handleWorld.first, handleWorld.second);
    const float screenXHandle = toolbarWidth_ + handleScreen.first;
    const float screenYHandle = handleScreen.second;
    const float dx = static_cast<float>(screenX) - screenXHandle;
    const float dy = static_cast<float>(screenY) - screenYHandle;
    const float radius = kRotationHandleScreenRadiusPx + kRotationHandleHitPaddingPx;
    return (dx * dx + dy * dy) <= radius * radius;
}

bool EditorApp::IsPointNearResizeHandle(const BoundsInfo& bounds, int screenX, int screenY) const {
    auto checkHandles = [&](const auto& handles) -> bool {
        for (const auto& descriptor : handles) {
            const auto local = ResizeHandleLocalPosition(bounds, descriptor);
            const auto world = RotateLocalToWorld(bounds, local.first, local.second);
            const auto screen = viewport_.WorldToScreen(world.first, world.second);
            const float screenXHandle = toolbarWidth_ + screen.first;
            const float screenYHandle = screen.second;
            const float dx = static_cast<float>(screenX) - screenXHandle;
            const float dy = static_cast<float>(screenY) - screenYHandle;
            if ((dx * dx + dy * dy) <= kResizeHandleHitRadiusPx * kResizeHandleHitRadiusPx) {
                return true;
            }
        }
        return false;
    };

    if (bounds.shape == BoundsShape::kCircle) {
        return checkHandles(kResizeCircleHandles);
    }
    return checkHandles(kResizeBoxHandles);
}

}  // namespace level_editor

