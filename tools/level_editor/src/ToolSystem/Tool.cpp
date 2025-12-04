#include "level_editor/ToolSystem/Tool.h"

#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>
#include <nlohmann/json.hpp>

#include "level_editor/AssetCache.h"
#include "level_editor/LevelState.h"
#include "level_editor/SchemaService.h"
#include "level_editor/SpecialComponentHandlers/MapInteractionFactory.h"

namespace level_editor {

namespace {
constexpr float kMoveToolDefaultSize = 48.0f;
constexpr float kRotationHandleScreenOffset = 24.0f;
constexpr float kRotationHandleScreenRadius = 14.0f;
constexpr float kRotationHandleHitPadding = 10.0f;
constexpr float kRotationSnapDegrees = 15.0f;
constexpr float kDegPerRad = 180.0f / 3.14159265358979323846f;
constexpr float kRadPerDeg = 3.14159265358979323846f / 180.0f;
constexpr float kResizeHandleScreenSize = 9.0f;
constexpr float kResizeHandleHitRadius = 11.0f;

std::pair<float, float> RotateLocalToWorld(const BoundsInfo& bounds, float localX, float localY) {
    const float rad = bounds.angleDegrees * kRadPerDeg;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const float worldX = bounds.centerX + localX * cosA - localY * sinA;
    const float worldY = bounds.centerY + localX * sinA + localY * cosA;
    return {worldX, worldY};
}

std::pair<float, float> RotateWorldToLocal(const BoundsInfo& bounds, float worldX, float worldY) {
    const float dx = worldX - bounds.centerX;
    const float dy = worldY - bounds.centerY;
    const float rad = -bounds.angleDegrees * kRadPerDeg;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    const float localX = dx * cosA - dy * sinA;
    const float localY = dx * sinA + dy * cosA;
    return {localX, localY};
}

void DrawRotatedBounds(SDL_Renderer* renderer, const ViewportController* viewport, const BoundsInfo& bounds, SDL_Color color) {
    const float halfW = bounds.width * 0.5f;
    const float halfH = bounds.height * 0.5f;
    const std::pair<float, float> worldPoints[4] = {
        RotateLocalToWorld(bounds, -halfW, -halfH),
        RotateLocalToWorld(bounds, halfW, -halfH),
        RotateLocalToWorld(bounds, halfW, halfH),
        RotateLocalToWorld(bounds, -halfW, halfH)};

    SDL_FPoint pts[5];
    for (int i = 0; i < 4; ++i) {
        const auto screen = viewport->WorldToScreen(worldPoints[i].first, worldPoints[i].second);
        pts[i] = {screen.first, screen.second};
    }
    pts[4] = pts[0];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLinesF(renderer, pts, 5);
}

}  // namespace

namespace tool_utils {

const nlohmann::json* FindComponent(const nlohmann::json& object, const char* type) {
    if (!object.contains("components") || !object["components"].is_array()) {
        return nullptr;
    }
    for (const auto& component : object["components"]) {
        if (component.contains("type") && component["type"].is_string() && component["type"].get<std::string>() == type) {
            return &component;
        }
    }
    return nullptr;
}

nlohmann::json* FindComponent(nlohmann::json& object, const char* type) {
    if (!object.contains("components") || !object["components"].is_array()) {
        return nullptr;
    }
    for (auto& component : object["components"]) {
        if (component.contains("type") && component["type"].is_string() && component["type"].get<std::string>() == type) {
            return &component;
        }
    }
    return nullptr;
}

const nlohmann::json* TemplateComponentDefaults(const SchemaService* schemaService,
                                                const nlohmann::json& object,
                                                const char* type) {
    if (!schemaService) {
        return nullptr;
    }
    const std::string templateId = object.value("template", std::string{});
    if (templateId.empty()) {
        return nullptr;
    }
    return schemaService->GetTemplateComponentDefaults(templateId, type);
}

const nlohmann::json* FindObjectByName(const LevelState* state, const std::string& name) {
    if (!state) {
        return nullptr;
    }
    const auto& doc = state->GetDocument().data;
    const auto objectsIt = doc.find("objects");
    if (objectsIt == doc.end() || !objectsIt->is_array()) {
        return nullptr;
    }
    for (const auto& object : *objectsIt) {
        if (object.value("name", std::string{}) == name) {
            return &object;
        }
    }
    return nullptr;
}

nlohmann::json* FindObjectByName(LevelState* state, const std::string& name) {
    if (!state) {
        return nullptr;
    }
    auto& doc = state->GetDocument().data;
    auto objectsIt = doc.find("objects");
    if (objectsIt == doc.end() || !objectsIt->is_array()) {
        return nullptr;
    }
    for (auto& object : *objectsIt) {
        if (object.value("name", std::string{}) == name) {
            return &object;
        }
    }
    return nullptr;
}

bool ComputeBounds(const nlohmann::json& object,
                   BoundsInfo& out,
                   const SchemaService* schemaService,
                   AssetCache* assetCache) {
    out.name = object.value("name", std::string{});
    const auto* body = FindComponent(object, "BodyComponent");
    const auto* templateBody = TemplateComponentDefaults(schemaService, object, "BodyComponent");
    const auto* sprite = FindComponent(object, "SpriteComponent");
    const auto* templateSprite = TemplateComponentDefaults(schemaService, object, "SpriteComponent");
    out.angleDegrees = 0.0f;
    if (body && body->contains("angle") && (*body)["angle"].is_number()) {
        out.angleDegrees = (*body)["angle"].get<float>();
    } else if (templateBody && templateBody->contains("angle") && (*templateBody)["angle"].is_number()) {
        out.angleDegrees = (*templateBody)["angle"].get<float>();
    }

    auto readBodyFloat = [&](const char* key, float fallback) -> float {
        if (body && body->contains(key) && (*body)[key].is_number()) {
            return (*body)[key].get<float>();
        }
        if (templateBody && templateBody->contains(key) && (*templateBody)[key].is_number()) {
            return (*templateBody)[key].get<float>();
        }
        return fallback;
    };
    auto readFixtureFloat = [&](const char* key, float fallback) -> float {
        if (body && body->contains("fixture")) {
            const auto& fixture = (*body)["fixture"];
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
        if (body && body->contains("fixture")) {
            const auto& fixture = (*body)["fixture"];
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
    auto readSpriteFloat = [&](const char* key, float fallback) -> float {
        if (sprite && sprite->contains(key) && (*sprite)[key].is_number()) {
            return (*sprite)[key].get<float>();
        }
        if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_number()) {
            return (*templateSprite)[key].get<float>();
        }
        return fallback;
    };
    auto readSpriteString = [&](const char* key, const std::string& fallback) -> std::string {
        if (sprite && sprite->contains(key) && (*sprite)[key].is_string()) {
            return (*sprite)[key].get<std::string>();
        }
        if (templateSprite && templateSprite->contains(key) && (*templateSprite)[key].is_string()) {
            return (*templateSprite)[key].get<std::string>();
        }
        return fallback;
    };

    const float spritePosX = readSpriteFloat("posX", 0.0f);
    const float spritePosY = readSpriteFloat("posY", 0.0f);
    out.centerX = readBodyFloat("posX", spritePosX);
    out.centerY = readBodyFloat("posY", spritePosY);

    float width = readFixtureFloat("width", 0.0f);
    float height = readFixtureFloat("height", 0.0f);
    const std::string shape = readFixtureString("shape", "box");
    out.shape = shape == "circle" ? BoundsShape::kCircle : BoundsShape::kBox;
    if (out.shape == BoundsShape::kCircle) {
        float radius = readFixtureFloat("radius", 0.0f);
        if (radius <= 0.0f) {
            radius = std::max(width, height) * 0.5f;
        }
        if (radius <= 0.0f) {
            radius = kMoveToolDefaultSize * 0.5f;
        }
        out.radius = radius;
        width = height = radius * 2.0f;
    } else {
        out.radius = 0.0f;
    }

    const float renderWidth = readSpriteFloat("renderWidth", 0.0f);
    const float renderHeight = readSpriteFloat("renderHeight", 0.0f);
    if (renderWidth > 0.0f) {
        width = renderWidth;
    }
    if (renderHeight > 0.0f) {
        height = renderHeight;
    }

    if ((width <= 0.0f || height <= 0.0f) && assetCache) {
        std::string spriteName = readSpriteString("spriteName", std::string{});
        if (spriteName.empty()) {
            spriteName = object.value("template", spriteName);
        }
        if (!spriteName.empty()) {
            if (const auto* spriteInfo = assetCache->GetSpriteInfo(spriteName); spriteInfo && !spriteInfo->frames.empty()) {
                const auto& frame = spriteInfo->frames[0];
                if (width <= 0.0f) {
                    width = static_cast<float>(frame.w);
                }
                if (height <= 0.0f) {
                    height = static_cast<float>(frame.h);
                }
            }
        }
    }

    if (width <= 0.0f) {
        width = kMoveToolDefaultSize;
    }
    if (height <= 0.0f) {
        height = kMoveToolDefaultSize;
    }
    if (out.shape == BoundsShape::kCircle && out.radius <= 0.0f) {
        out.radius = std::max(width, height) * 0.5f;
    }
    out.width = width;
    out.height = height;
    return true;
}

bool PointInBounds(const BoundsInfo& bounds, float worldX, float worldY) {
    if (bounds.shape == BoundsShape::kCircle) {
        const float dist = std::hypot(worldX - bounds.centerX, worldY - bounds.centerY);
        return dist <= bounds.radius;
    }
    const auto local = RotateWorldToLocal(bounds, worldX, worldY);
    const float halfW = bounds.width * 0.5f;
    const float halfH = bounds.height * 0.5f;
    return std::abs(local.first) <= halfW && std::abs(local.second) <= halfH;
}

std::string PickObject(LevelState* state,
                       float worldX,
                       float worldY,
                       const SchemaService* schemaService,
                       AssetCache* assetCache) {
    if (!state) {
        return {};
    }
    auto& doc = state->GetDocument().data;
    auto objectsIt = doc.find("objects");
    if (objectsIt == doc.end() || !objectsIt->is_array()) {
        return {};
    }
    auto& array = *objectsIt;
    for (std::size_t i = array.size(); i-- > 0;) {
        const auto& object = array.at(i);
        BoundsInfo bounds;
        if (ComputeBounds(object, bounds, schemaService, assetCache) && PointInBounds(bounds, worldX, worldY)) {
            return bounds.name;
        }
    }
    return {};
}

}  // namespace tool_utils

using namespace tool_utils;

namespace {

struct SizeConstraints {
    float minWidth{4.0f};
    float maxWidth{4096.0f};
    float minHeight{4.0f};
    float maxHeight{4096.0f};
    float minRadius{2.0f};
    float maxRadius{2048.0f};
};

SizeConstraints BuildSizeConstraints(const SchemaService* schemaService) {
    SizeConstraints constraints;
    if (!schemaService) {
        return constraints;
    }
    if (const auto* descriptor = schemaService->GetComponentDescriptor("BodyComponent"); descriptor) {
        for (const auto& field : descriptor->fields) {
            auto applyRange = [&](const FieldDescriptor& desc, float& minVal, float& maxVal) {
                if (desc.minValue) {
                    minVal = static_cast<float>(*desc.minValue);
                }
                if (desc.maxValue) {
                    maxVal = static_cast<float>(*desc.maxValue);
                }
            };
            if (field.name == "width") {
                applyRange(field, constraints.minWidth, constraints.maxWidth);
            } else if (field.name == "height") {
                applyRange(field, constraints.minHeight, constraints.maxHeight);
            } else if (field.name == "radius") {
                applyRange(field, constraints.minRadius, constraints.maxRadius);
            }
        }
    }
    constraints.maxWidth = std::max(constraints.maxWidth, constraints.minWidth);
    constraints.maxHeight = std::max(constraints.maxHeight, constraints.minHeight);
    constraints.maxRadius = std::max(constraints.maxRadius, constraints.minRadius);
    return constraints;
}

struct HandleDescriptor {
    ResizeTool::Handle handle;
    int dirX;
    int dirY;
};

const std::array<HandleDescriptor, 8> kBoxHandles = {
    HandleDescriptor{ResizeTool::Handle::kNorth, 0, -1},
    HandleDescriptor{ResizeTool::Handle::kSouth, 0, 1},
    HandleDescriptor{ResizeTool::Handle::kEast, 1, 0},
    HandleDescriptor{ResizeTool::Handle::kWest, -1, 0},
    HandleDescriptor{ResizeTool::Handle::kNorthEast, 1, -1},
    HandleDescriptor{ResizeTool::Handle::kNorthWest, -1, -1},
    HandleDescriptor{ResizeTool::Handle::kSouthEast, 1, 1},
    HandleDescriptor{ResizeTool::Handle::kSouthWest, -1, 1},
};

const std::array<HandleDescriptor, 4> kCircleHandles = {
    HandleDescriptor{ResizeTool::Handle::kNorth, 0, -1},
    HandleDescriptor{ResizeTool::Handle::kSouth, 0, 1},
    HandleDescriptor{ResizeTool::Handle::kEast, 1, 0},
    HandleDescriptor{ResizeTool::Handle::kWest, -1, 0},
};

std::pair<float, float> HandleLocalPosition(const BoundsInfo& bounds, const HandleDescriptor& desc) {
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

HandleDescriptor LookupDescriptor(ResizeTool::Handle handle) {
    for (const auto& entry : kBoxHandles) {
        if (entry.handle == handle) {
            return entry;
        }
    }
    return HandleDescriptor{ResizeTool::Handle::kNone, 0, 0};
}

}  // namespace

void ITool::OnActivated(const ToolContext& /*ctx*/) {}

void ITool::OnDeactivated(const ToolContext& /*ctx*/) {}

void ITool::OnInput(const ToolContext& /*ctx*/, const InputEvent& /*event*/) {}

void ITool::OnRender(const ToolContext& /*ctx*/, const RenderContext& /*context*/) {}

PlacementTool::PlacementTool() = default;

void PlacementTool::SetSchemaService(const SchemaService* schemaService) {
    schemaService_ = schemaService;
}

void PlacementTool::SetLevelState(LevelState* levelState) {
    levelState_ = levelState;
}

void PlacementTool::SetActiveTemplate(std::string templateId) {
    activeTemplateId_ = std::move(templateId);
}

void PlacementTool::OnInput(const ToolContext& ctx, const InputEvent& event) {
    if (event.type != InputEvent::Type::kMouseDown || event.mouseButton != MouseButton::kLeft) {
        return;
    }
    if (!ctx.viewport) {
        return;
    }
    const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
    PlaceObjectAt(world.first, world.second);
}

bool PlacementTool::PlaceObjectAt(float worldX, float worldY) {
    return PlaceInternal(worldX, worldY);
}

bool PlacementTool::PlaceInternal(float worldX, float worldY) {
    if (!levelState_ || !schemaService_ || activeTemplateId_.empty()) {
        return false;
    }

    const std::string name = levelState_->GenerateUniqueObjectName(activeTemplateId_);

    nlohmann::json object;
    object["name"] = name;
    object["template"] = activeTemplateId_;
    object["components"] = nlohmann::json::array();

    nlohmann::json bodyComponent;
    bodyComponent["type"] = "BodyComponent";
    bodyComponent["posX"] = worldX;
    bodyComponent["posY"] = worldY;
    object["components"].push_back(bodyComponent);

    levelState_->AppendObject(object);
    levelState_->SetSelection({name});
    return true;
}

MoveRotateTool::MoveRotateTool() = default;

void MoveRotateTool::SetSchemaService(const SchemaService* schemaService) {
    schemaService_ = schemaService;
}

void MoveRotateTool::SetLevelState(LevelState* levelState) {
    levelState_ = levelState;
}

void MoveRotateTool::SetAssetCache(AssetCache* assetCache) {
    assetCache_ = assetCache;
}

void MoveRotateTool::OnInput(const ToolContext& ctx, const InputEvent& event) {
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state || !ctx.viewport) {
        return;
    }

    switch (event.type) {
        case InputEvent::Type::kMouseDown:
            if (event.mouseButton == MouseButton::kLeft) {
                const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
                BoundsInfo bounds;
                std::string targetName;
                std::cout << "[MoveRotate] MouseDown screen(" << event.x << ", " << event.y << ") world(" << world.first << ", "
                          << world.second << ")\n";

                bool clickedHandle = false;
                const auto& selection = state->GetSelection();
                if (!selection.empty()) {
                    const auto* selected = FindObjectByName(state, selection.front());
                    if (selected && ComputeBounds(*selected, bounds, schemaService_, assetCache_)) {
                        const float zoom = ctx.viewport->GetCamera().zoom;
                        const auto handlePos = RotationHandleWorldPos(bounds, zoom);
                        const float handleRadiusWorld =
                            (kRotationHandleScreenRadius + kRotationHandleHitPadding) / std::max(zoom, 0.001f);
                        const float distToHandle = std::hypot(world.first - handlePos.first, world.second - handlePos.second);
                        std::cout << "[MoveRotate] Selection handle world(" << handlePos.first << ", " << handlePos.second
                                  << ") mouse dist=" << distToHandle << " threshold=" << handleRadiusWorld << "\n";
                        if (distToHandle <= handleRadiusWorld) {
                            targetName = selection.front();
                            clickedHandle = true;
                        } else if (PointInBounds(bounds, world.first, world.second)) {
                            targetName = selection.front();
                        }
                    }
                }
                if (targetName.empty()) {
                    targetName = PickObject(state, world.first, world.second, schemaService_, assetCache_);
                }
                if (targetName.empty()) {
                    std::cout << "[MoveRotate] No object hit under cursor\n";
                    break;
                }

                const auto* target = FindObjectByName(state, targetName);
                if (!target || !ComputeBounds(*target, bounds, schemaService_, assetCache_)) {
                    std::cout << "[MoveRotate] Failed to compute bounds for " << targetName << "\n";
                    break;
                }

                std::cout << "[MoveRotate] Selected object " << targetName << " center(" << bounds.centerX << ", " << bounds.centerY
                          << ") size(" << bounds.width << ", " << bounds.height << ") angle " << bounds.angleDegrees << "\n";
                state->SetSelection({targetName});
                dragging_ = false;
                rotating_ = false;
                activeObjectName_ = targetName;
                const float zoom = ctx.viewport->GetCamera().zoom;
                const auto handlePos = RotationHandleWorldPos(bounds, zoom);
                const float handleRadiusWorld =
                    (kRotationHandleScreenRadius + kRotationHandleHitPadding) / std::max(zoom, 0.001f);
                const float distToHandle = std::hypot(world.first - handlePos.first, world.second - handlePos.second);
                std::cout << "[MoveRotate] Handle world(" << handlePos.first << ", " << handlePos.second
                          << ") mouse dist=" << distToHandle << " threshold=" << handleRadiusWorld << "\n";
                if (clickedHandle || distToHandle <= handleRadiusWorld) {
                    rotating_ = true;
                    dragging_ = false;
                    initialMouseAngleRad_ = std::atan2(world.second - bounds.centerY, world.first - bounds.centerX);
                    initialBodyAngleDeg_ = ReadBodyAngle(*target);
                    std::cout << "[MoveRotate] Enter rotating mode. initialMouseAngleRad=" << initialMouseAngleRad_
                              << " initialBodyAngleDeg=" << initialBodyAngleDeg_ << "\n";
                } else if (PointInBounds(bounds, world.first, world.second)) {
                    dragging_ = true;
                    dragOffsetX_ = world.first - bounds.centerX;
                    dragOffsetY_ = world.second - bounds.centerY;
                    std::cout << "[MoveRotate] Enter dragging mode. dragOffset(" << dragOffsetX_ << ", " << dragOffsetY_ << ")\n";
                } else {
                    dragging_ = true;
                    dragOffsetX_ = world.first - bounds.centerX;
                    dragOffsetY_ = world.second - bounds.centerY;
                    std::cout << "[MoveRotate] Enter dragging mode (outside bounds). dragOffset(" << dragOffsetX_ << ", "
                              << dragOffsetY_ << ")\n";
                }
            }
            break;
        case InputEvent::Type::kMouseMove:
            if (dragging_ && !activeObjectName_.empty()) {
                const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
                const float newX = world.first - dragOffsetX_;
                const float newY = world.second - dragOffsetY_;
                ApplyPosition(state, activeObjectName_, newX, newY);
            } else if (rotating_ && !activeObjectName_.empty()) {
                const auto* target = FindObjectByName(state, activeObjectName_);
                if (!target) {
                    break;
                }
                BoundsInfo bounds;
                if (!ComputeBounds(*target, bounds, schemaService_, assetCache_)) {
                    break;
                }
                const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
                const float currentMouseAngle = std::atan2(world.second - bounds.centerY, world.first - bounds.centerX);
                float deltaDeg = (currentMouseAngle - initialMouseAngleRad_) * kDegPerRad;
                float newAngle = initialBodyAngleDeg_ + deltaDeg;
                newAngle = std::round(newAngle / kRotationSnapDegrees) * kRotationSnapDegrees;
                while (newAngle > 180.0f) {
                    newAngle -= 360.0f;
                }
                while (newAngle <= -180.0f) {
                    newAngle += 360.0f;
                }
                ApplyAngle(state, activeObjectName_, newAngle);
            }
            break;
        case InputEvent::Type::kMouseUp:
            if (event.mouseButton == MouseButton::kLeft) {
                if (dragging_) {
                    dragging_ = false;
                }
                if (rotating_) {
                    rotating_ = false;
                }
                activeObjectName_.clear();
            }
            break;
        default:
            break;
    }
}

void MoveRotateTool::OnRender(const ToolContext& ctx, const RenderContext& context) {
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state || !ctx.viewport || !context.renderer) {
        return;
    }

    const auto& selection = state->GetSelection();
    if (selection.empty()) {
        return;
    }

    SDL_SetRenderDrawColor(context.renderer, 255, 210, 64, 255);
    const float zoom = ctx.viewport->GetCamera().zoom;

    for (const auto& name : selection) {
        const auto* object = FindObjectByName(state, name);
        if (!object) {
            continue;
        }
        BoundsInfo bounds;
        if (!ComputeBounds(*object, bounds, schemaService_, assetCache_)) {
            continue;
        }

        SDL_Color outlineColor{255, 210, 64, 255};
        DrawRotatedBounds(context.renderer, ctx.viewport, bounds, outlineColor);

        const auto handleWorld = RotationHandleWorldPos(bounds, zoom);
        const auto handleTop = ctx.viewport->WorldToScreen(handleWorld.first, handleWorld.second);
        const float localTopOffset = -(bounds.shape == BoundsShape::kCircle ? bounds.radius : bounds.height * 0.5f);
        const auto topCenterWorld = RotateLocalToWorld(bounds, 0.0f, localTopOffset);
        const auto topCenter = ctx.viewport->WorldToScreen(topCenterWorld.first, topCenterWorld.second);
        SDL_RenderDrawLineF(context.renderer, topCenter.first, topCenter.second, handleTop.first, handleTop.second);
        SDL_FRect handleRect{handleTop.first - 5.0f, handleTop.second - 5.0f, 10.0f, 10.0f};
        SDL_RenderFillRectF(context.renderer, &handleRect);

        const float corners[4][2] = {
            {-bounds.width * 0.5f, -bounds.height * 0.5f},
            {bounds.width * 0.5f, -bounds.height * 0.5f},
            {bounds.width * 0.5f, bounds.height * 0.5f},
            {-bounds.width * 0.5f, bounds.height * 0.5f}};
        for (const auto& corner : corners) {
            const auto worldCorner = RotateLocalToWorld(bounds, corner[0], corner[1]);
            const auto screen = ctx.viewport->WorldToScreen(worldCorner.first, worldCorner.second);
            SDL_FRect cornerRect{screen.first - 4.0f, screen.second - 4.0f, 8.0f, 8.0f};
            SDL_RenderFillRectF(context.renderer, &cornerRect);
        }
    }
}


void MoveRotateTool::ApplyPosition(LevelState* state, const std::string& objectName, float newX, float newY) {
    if (!state) {
        return;
    }
    auto* object = FindObjectByName(state, objectName);
    if (!object) {
        return;
    }
    auto* body = FindComponent(*object, "BodyComponent");
    auto* sprite = FindComponent(*object, "SpriteComponent");
    bool updated = false;
    if (body) {
        (*body)["posX"] = newX;
        (*body)["posY"] = newY;
        updated = true;
    }
    if (sprite) {
        (*sprite)["posX"] = newX;
        (*sprite)["posY"] = newY;
        updated = true;
    }
    if (updated) {
        state->MarkDirty();
    }
}

float MoveRotateTool::ReadBodyAngle(const nlohmann::json& object) const {
    const auto* body = FindComponent(object, "BodyComponent");
    if (body && body->contains("angle") && (*body)["angle"].is_number()) {
        return (*body)["angle"].get<float>();
    }
    if (const auto* defaults = TemplateComponentDefaults(schemaService_, object, "BodyComponent"); defaults &&
        defaults->contains("angle") &&
        (*defaults)["angle"].is_number()) {
        return (*defaults)["angle"].get<float>();
    }
    return 0.0f;
}

void MoveRotateTool::ApplyAngle(LevelState* state, const std::string& objectName, float newAngleDegrees) {
    if (!state) {
        return;
    }
    auto* object = FindObjectByName(state, objectName);
    if (!object) {
        return;
    }
    bool updated = false;
    if (auto* body = FindComponent(*object, "BodyComponent")) {
        (*body)["angle"] = newAngleDegrees;
        updated = true;
    }
    if (auto* sprite = FindComponent(*object, "SpriteComponent")) {
        (*sprite)["angle"] = newAngleDegrees;
        updated = true;
    }
    if (updated) {
        state->MarkDirty();
    }
}

std::pair<float, float> MoveRotateTool::RotationHandleWorldPos(const BoundsInfo& bounds, float zoom) const {
    const float offsetWorld = kRotationHandleScreenOffset / std::max(zoom, 0.001f);
    const float extent = bounds.shape == BoundsShape::kCircle ? bounds.radius : bounds.height * 0.5f;
    return RotateLocalToWorld(bounds, 0.0f, -(extent + offsetWorld));
}

ResizeTool::ResizeTool() = default;

void ResizeTool::SetSchemaService(const SchemaService* schemaService) {
    schemaService_ = schemaService;
}

void ResizeTool::SetLevelState(LevelState* levelState) {
    levelState_ = levelState;
}

void ResizeTool::SetAssetCache(AssetCache* assetCache) {
    assetCache_ = assetCache;
}

void ResizeTool::OnInput(const ToolContext& ctx, const InputEvent& event) {
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state) {
        return;
    }
    switch (event.type) {
        case InputEvent::Type::kMouseDown:
            if (event.mouseButton == MouseButton::kLeft) {
                BeginDrag(ctx, event);
            }
            break;
        case InputEvent::Type::kMouseMove:
            UpdateDrag(ctx, event);
            break;
        case InputEvent::Type::kMouseUp:
            if (event.mouseButton == MouseButton::kLeft) {
                EndDrag();
            }
            break;
        default:
            break;
    }
}

void ResizeTool::OnRender(const ToolContext& ctx, const RenderContext& context) {
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state || !ctx.viewport || !context.renderer) {
        return;
    }
    const auto& selection = state->GetSelection();
    if (selection.empty()) {
        return;
    }

    SDL_Color outlineColor{180, 220, 255, 255};

    for (const auto& name : selection) {
        const auto* object = FindObjectByName(state, name);
        if (!object) {
            continue;
        }
        BoundsInfo bounds;
        if (!ComputeBounds(*object, bounds, schemaService_, assetCache_)) {
            continue;
        }
        DrawRotatedBounds(context.renderer, ctx.viewport, bounds, outlineColor);
        DrawHandles(ctx, context, bounds);
    }
}

void ResizeTool::BeginDrag(const ToolContext& ctx, const InputEvent& event) {
    if (!ctx.viewport) {
        return;
    }
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state) {
        return;
    }
    const auto& selection = state->GetSelection();
    if (selection.empty()) {
        return;
    }
    const auto* object = FindObjectByName(state, selection.front());
    if (!object) {
        return;
    }
    BoundsInfo bounds;
    if (!ComputeBounds(*object, bounds, schemaService_, assetCache_)) {
        return;
    }

    const Handle handle = HitTestHandles(ctx, bounds, event.x, event.y);
    if (handle == Handle::kNone) {
        const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
        if (PointInBounds(bounds, world.first, world.second)) {
            state->SetSelection({selection.front()});
        }
        return;
    }

    const auto descriptor = LookupDescriptor(handle);
    drag_.active = true;
    drag_.handle = handle;
    drag_.objectName = selection.front();
    drag_.dirX = descriptor.dirX;
    drag_.dirY = descriptor.dirY;
    drag_.maintainAspect = bounds.shape == BoundsShape::kCircle;
}

void ResizeTool::UpdateDrag(const ToolContext& ctx, const InputEvent& event) {
    if (!drag_.active || event.type != InputEvent::Type::kMouseMove || !ctx.viewport) {
        return;
    }
    LevelState* state = ctx.levelState ? ctx.levelState : levelState_;
    if (!state) {
        EndDrag();
        return;
    }
    auto* object = FindObjectByName(state, drag_.objectName);
    if (!object) {
        EndDrag();
        return;
    }
    BoundsInfo bounds;
    if (!ComputeBounds(*object, bounds, schemaService_, assetCache_)) {
        EndDrag();
        return;
    }
    const auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
    ApplyResize(state, bounds, drag_.handle, world.first, world.second);
}

void ResizeTool::EndDrag() {
    drag_.active = false;
    drag_.handle = Handle::kNone;
    drag_.objectName.clear();
}

ResizeTool::Handle ResizeTool::HitTestHandles(const ToolContext& ctx, const BoundsInfo& bounds, int screenX, int screenY) const {
    if (!ctx.viewport) {
        return Handle::kNone;
    }

    const auto pointerWorld = ctx.viewport->ScreenToWorld(static_cast<float>(screenX), static_cast<float>(screenY));
    const float zoom = std::max(ctx.viewport->GetCamera().zoom, 0.001f);
    const float hitRadiusWorld = kResizeHandleHitRadius / zoom;

    auto testHandles = [&](const auto& handles) -> Handle {
        for (const auto& descriptor : handles) {
            const auto local = HandleLocalPosition(bounds, descriptor);
            const auto world = RotateLocalToWorld(bounds, local.first, local.second);
            const float dx = world.first - pointerWorld.first;
            const float dy = world.second - pointerWorld.second;
            const float dist = std::hypot(dx, dy);
            if (dist <= hitRadiusWorld) {
                return descriptor.handle;
            }
        }
        return Handle::kNone;
    };

    if (bounds.shape == BoundsShape::kCircle) {
        return testHandles(kCircleHandles);
    }
    return testHandles(kBoxHandles);
}

void ResizeTool::ApplyResize(LevelState* state, const BoundsInfo& bounds, Handle handle, float worldX, float worldY) {
    if (!state) {
        return;
    }
    auto* object = FindObjectByName(state, drag_.objectName);
    if (!object) {
        EndDrag();
        return;
    }

    auto* body = FindComponent(*object, "BodyComponent");
    auto* sprite = FindComponent(*object, "SpriteComponent");
    if (!body) {
        return;
    }

    auto& fixture = (*body)["fixture"];
    if (!fixture.is_object()) {
        fixture = nlohmann::json::object();
    }

    bool updated = false;
    auto constraints = BuildSizeConstraints(schemaService_);

    if (drag_.maintainAspect || bounds.shape == BoundsShape::kCircle) {
        const float dx = worldX - bounds.centerX;
        const float dy = worldY - bounds.centerY;
        float newRadius = std::hypot(dx, dy);
        newRadius = std::clamp(newRadius, constraints.minRadius, constraints.maxRadius);
        fixture["radius"] = newRadius;
        if (sprite) {
            (*sprite)["renderWidth"] = newRadius * 2.0f;
            (*sprite)["renderHeight"] = newRadius * 2.0f;
        }
        updated = true;
    } else {
        const auto mouseLocal = RotateWorldToLocal(bounds, worldX, worldY);
        float handleX = static_cast<float>(drag_.dirX) * bounds.width * 0.5f;
        float handleY = static_cast<float>(drag_.dirY) * bounds.height * 0.5f;
        float anchorX = -handleX;
        float anchorY = -handleY;

        float newWidth = bounds.width;
        float newHeight = bounds.height;
        float newCenterLocalX = 0.0f;
        float newCenterLocalY = 0.0f;

        if (drag_.dirX != 0) {
            const float sign = static_cast<float>(drag_.dirX);
            float extent = (mouseLocal.first - anchorX) * sign;
            extent = std::max(extent, 0.0f);
            extent = std::clamp(extent, constraints.minWidth, constraints.maxWidth);
            newWidth = extent;
            handleX = anchorX + sign * extent;
            newCenterLocalX = (handleX + anchorX) * 0.5f;
        }

        if (drag_.dirY != 0) {
            const float sign = static_cast<float>(drag_.dirY);
            float extent = (mouseLocal.second - anchorY) * sign;
            extent = std::max(extent, 0.0f);
            extent = std::clamp(extent, constraints.minHeight, constraints.maxHeight);
            newHeight = extent;
            handleY = anchorY + sign * extent;
            newCenterLocalY = (handleY + anchorY) * 0.5f;
        }

        const auto newCenterWorld = RotateLocalToWorld(bounds, newCenterLocalX, newCenterLocalY);
        (*body)["posX"] = newCenterWorld.first;
        (*body)["posY"] = newCenterWorld.second;
        fixture["width"] = newWidth;
        fixture["height"] = newHeight;

        if (sprite) {
            (*sprite)["posX"] = newCenterWorld.first;
            (*sprite)["posY"] = newCenterWorld.second;
            (*sprite)["renderWidth"] = newWidth;
            (*sprite)["renderHeight"] = newHeight;
        }
        updated = true;
    }

    if (updated) {
        state->MarkDirty();
    }
}

void ResizeTool::DrawHandles(const ToolContext& ctx, const RenderContext& context, const BoundsInfo& bounds) const {
    if (!ctx.viewport || !context.renderer) {
        return;
    }

    auto drawHandles = [&](const auto& handles) {
        for (const auto& descriptor : handles) {
            const auto local = HandleLocalPosition(bounds, descriptor);
            const auto world = RotateLocalToWorld(bounds, local.first, local.second);
            const auto screen = ctx.viewport->WorldToScreen(world.first, world.second);
            SDL_FRect rect{screen.first - kResizeHandleScreenSize * 0.5f,
                           screen.second - kResizeHandleScreenSize * 0.5f,
                           kResizeHandleScreenSize,
                           kResizeHandleScreenSize};
            if (drag_.active && drag_.handle == descriptor.handle) {
                SDL_SetRenderDrawColor(context.renderer, 255, 140, 0, 255);
            } else {
                SDL_SetRenderDrawColor(context.renderer, 245, 245, 245, 255);
            }
            SDL_RenderFillRectF(context.renderer, &rect);
        }
    };

    if (bounds.shape == BoundsShape::kCircle) {
        drawHandles(kCircleHandles);
    } else {
        drawHandles(kBoxHandles);
    }
}

void ToolController::SetViewport(ViewportController* viewport) {
    viewport_ = viewport;
    if (activeTool_ != ToolId::kNone) {
        if (auto it = tools_.find(activeTool_); it != tools_.end() && viewport_) {
            viewport_->SetTool(it->second);
        }
    }
}

void ToolController::RegisterTool(ToolId id, ToolPtr tool) {
    if (!tool) {
        tools_.erase(id);
        return;
    }
    tools_[id] = std::move(tool);
    if (activeTool_ == id && viewport_) {
        viewport_->SetTool(tools_[id]);
    }
}

ComponentEditTool::ComponentEditTool() = default;

void ComponentEditTool::BeginInteraction(MapInteractionRequest request, std::function<void()> onFinished) {
    if (session_) {
        CompleteInteraction(true);
    }
    session_ = CreateMapInteractionSession(request.type, request.payload);
    if (!session_) {
        if (request.onCancelled) {
            request.onCancelled();
        }
        return;
    }
    request_ = std::move(request);
    finishedCallback_ = std::move(onFinished);
}

void ComponentEditTool::CancelInteraction() {
    if (!session_) {
        return;
    }
    CompleteInteraction(true);
}

void ComponentEditTool::OnActivated(const ToolContext& /*ctx*/) {}

void ComponentEditTool::OnDeactivated(const ToolContext& /*ctx*/) {
    if (session_) {
        std::cout << "[ComponentEditTool] OnDeactivated forcing cancel\n";
        CompleteInteraction(true);
    }
}

void ComponentEditTool::OnInput(const ToolContext& ctx, const InputEvent& event) {
    if (!session_) {
        return;
    }
    session_->OnInput(ctx, event);
    if (session_->IsFinished()) {
        if (request_.onComplete) {
            request_.onComplete(session_->BuildResult());
        }
        CompleteInteraction(false);
    } else if (session_->IsCancelled()) {
        CompleteInteraction(true);
    }
}

void ComponentEditTool::OnRender(const ToolContext& ctx, const RenderContext& context) {
    if (session_) {
        session_->OnRender(ctx, context);
    }
}

void ComponentEditTool::CompleteInteraction(bool cancelled) {
    if (completing_) {
        return;
    }
    completing_ = true;
    auto cancelHandler = request_.onCancelled;
    auto finished = finishedCallback_;
    session_.reset();
    finishedCallback_ = nullptr;
    request_ = MapInteractionRequest{};
    if (finished) {
        finished();
    }
    if (cancelled && cancelHandler) {
        cancelHandler();
    }
    completing_ = false;
}

bool ToolController::ActivateTool(ToolId id) {
    if (activeTool_ == id) {
        return true;
    }
    const auto it = tools_.find(id);
    if (it == tools_.end() || !viewport_) {
        return false;
    }
    viewport_->SetTool(it->second);
    activeTool_ = id;
    return true;
}

ToolPtr ToolController::GetTool(ToolId id) const {
    if (const auto it = tools_.find(id); it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

}  // namespace level_editor

