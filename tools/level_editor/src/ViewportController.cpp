#include "level_editor/ViewportController.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <nlohmann/json.hpp>

#include "level_editor/AssetCache.h"
#include "level_editor/LevelState.h"
#include "level_editor/SchemaService.h"
#include "level_editor/ToolSystem/Tool.h"

namespace level_editor {

namespace {

constexpr float kObjectDefaultSize = 48.0f;

struct ObjectVisual {
    float x{0.0f};
    float y{0.0f};
    float width{kObjectDefaultSize};
    float height{kObjectDefaultSize};
    bool selected{false};
    std::string spriteName;
    uint8_t colorR{255};
    uint8_t colorG{255};
    uint8_t colorB{255};
    uint8_t alpha{255};
    float angleDegrees{0.0f};
    bool hasBody{false};
};

void ComputeRotatedCorners(float centerX,
                           float centerY,
                           float width,
                           float height,
                           float angleDegrees,
                           std::array<SDL_FPoint, 4>& out) {
    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;
    const float rad = angleDegrees * static_cast<float>(M_PI) / 180.0f;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const std::array<std::pair<float, float>, 4> local = {
        std::make_pair(-halfW, -halfH),
        std::make_pair(halfW, -halfH),
        std::make_pair(halfW, halfH),
        std::make_pair(-halfW, halfH)};
    for (size_t i = 0; i < local.size(); ++i) {
        const float rx = local[i].first * cosA - local[i].second * sinA;
        const float ry = local[i].first * sinA + local[i].second * cosA;
        out[i] = SDL_FPoint{centerX + rx, centerY + ry};
    }
}

void FillQuad(SDL_Renderer* renderer, const std::array<SDL_FPoint, 4>& corners, SDL_Color color) {
    SDL_Vertex vertices[6];
    auto setVertex = [&](int index, const SDL_FPoint& pt) {
        vertices[index].position = pt;
        vertices[index].tex_coord = SDL_FPoint{0.0f, 0.0f};
        vertices[index].color = color;
    };
    setVertex(0, corners[0]);
    setVertex(1, corners[1]);
    setVertex(2, corners[2]);
    setVertex(3, corners[0]);
    setVertex(4, corners[2]);
    setVertex(5, corners[3]);
    SDL_RenderGeometry(renderer, nullptr, vertices, 6, nullptr, 0);
}

void DrawQuadOutline(SDL_Renderer* renderer, const std::array<SDL_FPoint, 4>& corners, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < 4; ++i) {
        const auto& a = corners[i];
        const auto& b = corners[(i + 1) % 4];
        SDL_RenderDrawLineF(renderer, a.x, a.y, b.x, b.y);
    }
}

void DrawQuadCross(SDL_Renderer* renderer, const std::array<SDL_FPoint, 4>& corners, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(renderer, corners[0].x, corners[0].y, corners[2].x, corners[2].y);
    SDL_RenderDrawLineF(renderer, corners[1].x, corners[1].y, corners[3].x, corners[3].y);
}

}  // namespace

ViewportController::ViewportController() = default;

void ViewportController::HandleInput(const InputEvent& event) {
    const int localX = event.x - viewportOffsetX_;
    const int localY = event.y - viewportOffsetY_;
    switch (event.type) {
        case InputEvent::Type::kMouseDown:
            if (event.mouseButton == MouseButton::kRight) {
                rightMouseDown_ = true;
                cameraController_.BeginDrag(localX, localY);
            }
            break;
        case InputEvent::Type::kMouseUp:
            if (event.mouseButton == MouseButton::kRight) {
                rightMouseDown_ = false;
                cameraController_.EndDrag();
            }
            break;
        case InputEvent::Type::kMouseMove:
            if (rightMouseDown_) {
                cameraController_.DragTo(localX, localY);
            }
            lastMouseX_ = localX;
            lastMouseY_ = localY;
            break;
        case InputEvent::Type::kScroll:
            cameraController_.AdjustZoom(event.delta);
            break;
        case InputEvent::Type::kKeyDown:
        case InputEvent::Type::kKeyUp: {
            const bool pressed = event.type == InputEvent::Type::kKeyDown;
            switch (event.key) {
                case Key::kW:
                    cameraInputState_.moveUp = pressed;
                    break;
                case Key::kS:
                    cameraInputState_.moveDown = pressed;
                    break;
                case Key::kA:
                    cameraInputState_.moveLeft = pressed;
                    break;
                case Key::kD:
                    cameraInputState_.moveRight = pressed;
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

    if (activeTool_) {
        ToolContext ctx{};
        ctx.viewport = this;
        ctx.levelState = levelState_;
        ctx.schemaService = schemaService_;
        ctx.assetCache = assetCache_;
        activeTool_->OnInput(ctx, event);
    }
}

void ViewportController::Update(float deltaSeconds) {
    cameraController_.Update(cameraInputState_, deltaSeconds);
    camera_ = cameraController_.GetState();
}

void ViewportController::Render(const RenderContext& context) {
    if (!context.renderer) {
        return;
    }
    viewportWidth_ = context.viewportWidth;
    viewportHeight_ = context.viewportHeight;
    viewportOffsetX_ = context.offsetX;
    viewportOffsetY_ = context.offsetY;

    SDL_Rect viewportRect{viewportOffsetX_, viewportOffsetY_, viewportWidth_, viewportHeight_};
    SDL_RenderSetViewport(context.renderer, &viewportRect);

    DrawBackgrounds(context);
    DrawGrid(context);
    DrawObjects(context);

    if (activeTool_) {
        ToolContext ctx{};
        ctx.viewport = this;
        ctx.levelState = levelState_;
        ctx.schemaService = schemaService_;
        ctx.assetCache = assetCache_;
        activeTool_->OnRender(ctx, context);
    }
}

void ViewportController::SetTool(std::shared_ptr<ITool> tool) {
    if (activeTool_) {
        ToolContext ctx{};
        ctx.viewport = this;
        ctx.levelState = levelState_;
        activeTool_->OnDeactivated(ctx);
    }
    activeTool_ = std::move(tool);
    if (activeTool_) {
        ToolContext ctx{};
        ctx.viewport = this;
        ctx.levelState = levelState_;
        activeTool_->OnActivated(ctx);
    }
}

void ViewportController::SetLevelState(LevelState* levelState) {
    levelState_ = levelState;
}

void ViewportController::SetAssetCache(AssetCache* assetCache) {
    assetCache_ = assetCache;
}

void ViewportController::SetSchemaService(const SchemaService* schemaService) {
    schemaService_ = schemaService;
}

const CameraState& ViewportController::GetCamera() const {
    return camera_;
}

void ViewportController::SetCamera(const CameraState& state) {
    camera_ = state;
    cameraController_.SetState(state);
}

void ViewportController::OnViewportResized(int width, int height) {
    viewportWidth_ = width;
    viewportHeight_ = height;
}

void ViewportController::DrawGrid(const RenderContext& context) const {
    SDL_SetRenderDrawColor(context.renderer, 40, 40, 45, 255);

    const float gridSize = 64.0f;
    const auto camera = cameraController_.GetState();
    const float startXWorld = camera.positionX - (viewportWidth_ * 0.5f) / camera.zoom;
    const float endXWorld = camera.positionX + (viewportWidth_ * 0.5f) / camera.zoom;
    const float startYWorld = camera.positionY - (viewportHeight_ * 0.5f) / camera.zoom;
    const float endYWorld = camera.positionY + (viewportHeight_ * 0.5f) / camera.zoom;

    const int startX = static_cast<int>(std::floor(startXWorld / gridSize) * gridSize);
    const int endX = static_cast<int>(std::ceil(endXWorld / gridSize) * gridSize);
    const int startY = static_cast<int>(std::floor(startYWorld / gridSize) * gridSize);
    const int endY = static_cast<int>(std::ceil(endYWorld / gridSize) * gridSize);

    for (int x = startX; x <= endX; x += static_cast<int>(gridSize)) {
        const auto screen = WorldToScreen(static_cast<float>(x), startYWorld);
        const auto screenEnd = WorldToScreen(static_cast<float>(x), endYWorld);
        SDL_RenderDrawLine(context.renderer,
                           static_cast<int>(screen.first),
                           static_cast<int>(screen.second),
                           static_cast<int>(screenEnd.first),
                           static_cast<int>(screenEnd.second));
    }

    for (int y = startY; y <= endY; y += static_cast<int>(gridSize)) {
        const auto screen = WorldToScreen(startXWorld, static_cast<float>(y));
        const auto screenEnd = WorldToScreen(endXWorld, static_cast<float>(y));
        SDL_RenderDrawLine(context.renderer,
                           static_cast<int>(screen.first),
                           static_cast<int>(screen.second),
                           static_cast<int>(screenEnd.first),
                           static_cast<int>(screenEnd.second));
    }
}

void ViewportController::DrawObjects(const RenderContext& context) const {
    if (!levelState_) {
        return;
    }
    const auto& doc = levelState_->GetDocument().data;
    if (!doc.contains("objects") || !doc["objects"].is_array()) {
        return;
    }
    const auto& selection = levelState_->GetSelection();

    for (const auto& object : doc["objects"]) {
        ObjectVisual visual;
        if (object.contains("name") && object["name"].is_string()) {
            const std::string name = object["name"].get<std::string>();
            visual.selected = std::find(selection.begin(), selection.end(), name) != selection.end();
        }

        const std::string templateId = object.value("template", std::string{});
        const nlohmann::json* templateBody =
            (schemaService_ && !templateId.empty())
                ? schemaService_->GetTemplateComponentDefaults(templateId, "BodyComponent")
                : nullptr;
        const nlohmann::json* templateSprite =
            (schemaService_ && !templateId.empty())
                ? schemaService_->GetTemplateComponentDefaults(templateId, "SpriteComponent")
                : nullptr;

        const nlohmann::json* bodyComponent = nullptr;
        const nlohmann::json* spriteComponent = nullptr;
        if (object.contains("components") && object["components"].is_array()) {
            for (const auto& component : object["components"]) {
                const std::string type = component.value("type", std::string{});
                if (type == "BodyComponent") {
                    bodyComponent = &component;
                } else if (type == "SpriteComponent") {
                    spriteComponent = &component;
                }
            }
        }

        auto readFloat = [](const nlohmann::json* node, const char* key, float fallback) -> float {
            if (node && node->contains(key) && (*node)[key].is_number()) {
                return (*node)[key].get<float>();
            }
            return fallback;
        };
        auto readInt = [](const nlohmann::json* node, const char* key, int fallback) -> int {
            if (node && node->contains(key) && (*node)[key].is_number_integer()) {
                return (*node)[key].get<int>();
            }
            return fallback;
        };
        auto readString = [](const nlohmann::json* node, const char* key, const std::string& fallback) -> std::string {
            if (node && node->contains(key) && (*node)[key].is_string()) {
                return (*node)[key].get<std::string>();
            }
            return fallback;
        };
        auto readBodyFloat = [&](const char* key, float fallback) -> float {
            if (bodyComponent && bodyComponent->contains(key) && (*bodyComponent)[key].is_number()) {
                return (*bodyComponent)[key].get<float>();
            }
            if (templateBody && templateBody->contains(key) && (*templateBody)[key].is_number()) {
                return (*templateBody)[key].get<float>();
            }
            return fallback;
        };
        auto readSpriteFloat = [&](const char* key, float fallback) -> float {
            if (spriteComponent && spriteComponent->contains(key) && (*spriteComponent)[key].is_number()) {
                return (*spriteComponent)[key].get<float>();
            }
            if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_number()) {
                return (*templateSprite)[key].get<float>();
            }
            return fallback;
        };
        auto readSpriteInt = [&](const char* key, int fallback) -> int {
            if (spriteComponent && spriteComponent->contains(key) && (*spriteComponent)[key].is_number_integer()) {
                return (*spriteComponent)[key].get<int>();
            }
            if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_number_integer()) {
                return (*templateSprite)[key].get<int>();
            }
            return fallback;
        };
        auto readSpriteString = [&](const char* key, const std::string& fallback) -> std::string {
            if (spriteComponent && spriteComponent->contains(key) && (*spriteComponent)[key].is_string()) {
                return (*spriteComponent)[key].get<std::string>();
            }
            if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_string()) {
                return (*templateSprite)[key].get<std::string>();
            }
            return fallback;
        };
        auto readFixtureFloat = [&](const char* key, float fallback) -> float {
            if (bodyComponent && bodyComponent->contains("fixture")) {
                const auto& fixture = (*bodyComponent)["fixture"];
                if (fixture.contains(key) && fixture[key].is_number()) {
                    return fixture[key].get<float>();
                }
            }
            if (templateBody && templateBody->contains("fixture")) {
                const auto& fixture = (*templateBody)["fixture"];
                if (fixture.contains(key) && fixture[key].is_number()) {
                    return fixture[key].get<float>();
                }
            }
            return fallback;
        };
        auto readFixtureString = [&](const char* key, const std::string& fallback) -> std::string {
            if (bodyComponent && bodyComponent->contains("fixture")) {
                const auto& fixture = (*bodyComponent)["fixture"];
                if (fixture.contains(key) && fixture[key].is_string()) {
                    return fixture[key].get<std::string>();
                }
            }
            if (templateBody && templateBody->contains("fixture")) {
                const auto& fixture = (*templateBody)["fixture"];
                if (fixture.contains(key) && fixture[key].is_string()) {
                    return fixture[key].get<std::string>();
                }
            }
            return fallback;
        };

        visual.spriteName = readSpriteString("spriteName", "");
        if (visual.spriteName.empty() && schemaService_ && !templateId.empty()) {
            if (const auto* descriptor = schemaService_->GetTemplate(templateId)) {
                visual.spriteName = descriptor->spriteName.empty() ? visual.spriteName : descriptor->spriteName;
            }
        }
        if (visual.spriteName.empty() && !templateId.empty()) {
            visual.spriteName = templateId;
        }

        visual.colorR = static_cast<uint8_t>(std::clamp(readSpriteInt("colorR", 255), 0, 255));
        visual.colorG = static_cast<uint8_t>(std::clamp(readSpriteInt("colorG", 255), 0, 255));
        visual.colorB = static_cast<uint8_t>(std::clamp(readSpriteInt("colorB", 255), 0, 255));
        visual.alpha = static_cast<uint8_t>(std::clamp(readSpriteInt("alpha", 255), 0, 255));

        const float spritePosX = readSpriteFloat("posX", 0.0f);
        const float spritePosY = readSpriteFloat("posY", 0.0f);
        visual.x = readBodyFloat("posX", spritePosX);
        visual.y = readBodyFloat("posY", spritePosY);
        visual.angleDegrees = readBodyFloat("angle", readSpriteFloat("angle", 0.0f));
        visual.hasBody = bodyComponent != nullptr || templateBody != nullptr;

        float width = 0.0f;
        float height = 0.0f;
        if (visual.hasBody) {
            const std::string shape = readFixtureString("shape", "box");
            if (shape == "circle") {
                const float radius = readFixtureFloat("radius", visual.width * 0.5f);
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

        const SpriteFrameInfo* spriteFrame = nullptr;
        if (assetCache_ && !visual.spriteName.empty()) {
            const auto* spriteInfo = assetCache_->GetSpriteInfo(visual.spriteName);
            if (spriteInfo && !spriteInfo->frames.empty()) {
                spriteFrame = &spriteInfo->frames[0];
                if (width <= 0.0f) {
                    width = static_cast<float>(spriteFrame->w);
                }
                if (height <= 0.0f) {
                    height = static_cast<float>(spriteFrame->h);
                }
            }
        }
        if (width <= 0.0f) {
            width = kObjectDefaultSize;
        }
        if (height <= 0.0f) {
            height = kObjectDefaultSize;
        }
        visual.width = width;
        visual.height = height;

        const float screenWidth = visual.width * camera_.zoom;
        const float screenHeight = visual.height * camera_.zoom;
        const auto centerScreen = WorldToScreen(visual.x, visual.y);
        SDL_FRect dest{centerScreen.first - screenWidth * 0.5f,
                       centerScreen.second - screenHeight * 0.5f,
                       screenWidth,
                       screenHeight};

        std::array<SDL_FPoint, 4> cachedCorners{};
        bool hasCorners = false;
        auto ensureCorners = [&]() -> const std::array<SDL_FPoint, 4>& {
            if (!hasCorners) {
                ComputeRotatedCorners(centerScreen.first, centerScreen.second, screenWidth, screenHeight, visual.angleDegrees, cachedCorners);
                hasCorners = true;
            }
            return cachedCorners;
        };

        if (assetCache_ && spriteFrame) {
            SDL_Texture* texture = assetCache_->GetTexture(spriteFrame->textureName);
            if (texture) {
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_Rect src{spriteFrame->x, spriteFrame->y, spriteFrame->w, spriteFrame->h};
                SDL_SetTextureColorMod(texture, visual.colorR, visual.colorG, visual.colorB);
                SDL_SetTextureAlphaMod(texture, visual.alpha);
                SDL_FPoint origin{screenWidth * 0.5f, screenHeight * 0.5f};
                SDL_RenderCopyExF(context.renderer, texture, &src, &dest, visual.angleDegrees, &origin, SDL_FLIP_NONE);
            }
        } else {
            const auto& corners = ensureCorners();
            SDL_Color fill{255, 255, 255, static_cast<uint8_t>(std::min<int>(visual.alpha, 180))};
            FillQuad(context.renderer, corners, fill);
            SDL_Color cross{200, 200, 200, fill.a};
            DrawQuadCross(context.renderer, corners, cross);
        }

        if (visual.selected) {
            const auto& corners = ensureCorners();
            DrawQuadOutline(context.renderer, corners, SDL_Color{255, 210, 64, 255});
        }
    }
}

std::pair<float, float> ViewportController::WorldToScreen(float worldX, float worldY) const {
    const float screenX = (worldX - camera_.positionX) * camera_.zoom + viewportWidth_ * 0.5f;
    const float screenY = (worldY - camera_.positionY) * camera_.zoom + viewportHeight_ * 0.5f;
    return {screenX, screenY};
}

std::pair<float, float> ViewportController::ScreenToWorld(float screenX, float screenY) const {
    const float localX = screenX - static_cast<float>(viewportOffsetX_);
    const float localY = screenY - static_cast<float>(viewportOffsetY_);
    const float worldX = (localX - viewportWidth_ * 0.5f) / camera_.zoom + camera_.positionX;
    const float worldY = (localY - viewportHeight_ * 0.5f) / camera_.zoom + camera_.positionY;
    return {worldX, worldY};
}

std::pair<float, float> ViewportController::BackgroundToScreen(float worldX, float worldY, float parallaxX, float parallaxY) const {
    const float cameraX = camera_.positionX * parallaxX;
    const float cameraY = camera_.positionY * parallaxY;
    const float screenX = (worldX - cameraX) * camera_.zoom + viewportWidth_ * 0.5f;
    const float screenY = (worldY - cameraY) * camera_.zoom + viewportHeight_ * 0.5f;
    return {screenX, screenY};
}

void ViewportController::DrawBackgrounds(const RenderContext& context) const {
    if (!assetCache_ || !levelState_) {
        return;
    }
    const auto& doc = levelState_->GetDocument().data;
    const auto backgroundIt = doc.find("background");
    if (backgroundIt == doc.end()) {
        return;
    }
    const auto layersIt = backgroundIt->find("layers");
    if (layersIt == backgroundIt->end() || !layersIt->is_array()) {
        return;
    }

    struct LayerRef {
        float depth;
        const nlohmann::json* json;
    };
    std::vector<LayerRef> layers;
    layers.reserve(layersIt->size());
    for (const auto& layer : *layersIt) {
        layers.push_back(LayerRef{layer.value("depth", 0.0f), &layer});
    }
    std::sort(layers.begin(), layers.end(), [](const LayerRef& a, const LayerRef& b) { return a.depth < b.depth; });

    const float cameraCenterX = camera_.positionX;
    const float cameraCenterY = camera_.positionY;
    const float cameraZoom = camera_.zoom;
    const float viewHalfWidth = (viewportWidth_ * 0.5f) / cameraZoom;
    const float viewHalfHeight = (viewportHeight_ * 0.5f) / cameraZoom;
    const float worldViewMinX = cameraCenterX - viewHalfWidth;
    const float worldViewMaxX = cameraCenterX + viewHalfWidth;
    const float worldViewMinY = cameraCenterY - viewHalfHeight;
    const float worldViewMaxY = cameraCenterY + viewHalfHeight;

    for (const auto& layerRef : layers) {
        const auto& layer = *layerRef.json;
        const std::string spriteName = layer.value("spriteName", std::string{});
        if (spriteName.empty()) {
            continue;
        }
        const auto* spriteInfo = assetCache_->GetSpriteInfo(spriteName);
        if (!spriteInfo || spriteInfo->frames.empty()) {
            continue;
        }
        const auto& frame = spriteInfo->frames[0];
        SDL_Texture* texture = assetCache_->GetTexture(frame.textureName);
        if (!texture) {
            continue;
        }

        const bool tiled = layer.value("tiled", false);
        const float parallaxX = layer.value("parallaxX", 1.0f);
        const float parallaxY = layer.value("parallaxY", 1.0f);
        const float offsetX = layer.value("offsetX", 0.0f);
        const float offsetY = layer.value("offsetY", 0.0f);
        const int alpha = std::clamp(layer.value("alpha", 255), 0, 255);
        SDL_SetTextureAlphaMod(texture, static_cast<uint8_t>(alpha));

        const float layerCenterX = offsetX + cameraCenterX * parallaxX;
        const float layerCenterY = offsetY + cameraCenterY * parallaxY;

        if (tiled) {
            float tileWidth = layer.value("tileWidth", static_cast<float>(frame.w));
            float tileHeight = layer.value("tileHeight", static_cast<float>(frame.h));
            if (tileWidth <= 0.0f) {
                tileWidth = static_cast<float>(frame.w);
            }
            if (tileHeight <= 0.0f) {
                tileHeight = static_cast<float>(frame.h);
            }

            const float tileStartWorldX =
                std::floor((worldViewMinX - layerCenterX) / tileWidth) * tileWidth + layerCenterX;
            const float tileStartWorldY =
                std::floor((worldViewMinY - layerCenterY) / tileHeight) * tileHeight + layerCenterY;
            const float tileEndWorldX =
                std::ceil((worldViewMaxX - layerCenterX) / tileWidth) * tileWidth + layerCenterX;
            const float tileEndWorldY =
                std::ceil((worldViewMaxY - layerCenterY) / tileHeight) * tileHeight + layerCenterY;

            SDL_Rect src{frame.x, frame.y, frame.w, frame.h};
            const float destTileWidth = tileWidth * cameraZoom;
            const float destTileHeight = tileHeight * cameraZoom;

            for (float ty = tileStartWorldY; ty <= tileEndWorldY; ty += tileHeight) {
                for (float tx = tileStartWorldX; tx <= tileEndWorldX; tx += tileWidth) {
                    const auto topLeft = WorldToScreen(tx, ty);
                    SDL_FRect dest{topLeft.first, topLeft.second, destTileWidth, destTileHeight};
                    SDL_RenderCopyF(context.renderer, texture, &src, &dest);
                }
            }
        } else {
            float layerWidth = layer.value("width", static_cast<float>(frame.w));
            float layerHeight = layer.value("height", static_cast<float>(frame.h));
            if (layerWidth <= 0.0f) {
                layerWidth = static_cast<float>(frame.w);
            }
            if (layerHeight <= 0.0f) {
                layerHeight = static_cast<float>(frame.h);
            }
            const float topLeftWorldX = layerCenterX - layerWidth * 0.5f;
            const float topLeftWorldY = layerCenterY - layerHeight * 0.5f;
            const auto topLeft = WorldToScreen(topLeftWorldX, topLeftWorldY);
            SDL_Rect src{frame.x, frame.y, frame.w, frame.h};
            SDL_FRect dest{topLeft.first,
                           topLeft.second,
                           layerWidth * cameraZoom,
                           layerHeight * cameraZoom};
            SDL_RenderCopyF(context.renderer, texture, &src, &dest);
        }
    }
}

}  // namespace level_editor
