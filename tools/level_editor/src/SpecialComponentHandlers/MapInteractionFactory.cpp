#include "level_editor/SpecialComponentHandlers/MapInteractionFactory.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "level_editor/LevelState.h"
#include "level_editor/ToolSystem/Tool.h"

namespace level_editor {

namespace {

constexpr float kPointHitRadius = 16.0f;
constexpr float kPi = 3.14159265358979323846f;

std::pair<float, float> ScreenToWorld(const ToolContext& ctx, const InputEvent& event) {
    if (!ctx.viewport) {
        return {0.0f, 0.0f};
    }
    return ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
}

bool IsWithinRadius(const ToolContext& ctx, const SDL_FPoint& point, float worldX, float worldY) {
    const float dx = point.x - worldX;
    const float dy = point.y - worldY;
    const float dist = std::sqrt(dx * dx + dy * dy);
    return dist <= kPointHitRadius / std::max(ctx.viewport ? ctx.viewport->GetCamera().zoom : 1.0f, 0.001f);
}

void DrawCircle(SDL_Renderer* renderer, float x, float y, float radius) {
    const int segments = 24;
    for (int i = 0; i < segments; ++i) {
        const float t0 = (static_cast<float>(i) / segments) * 2.0f * kPi;
        const float t1 = (static_cast<float>(i + 1) / segments) * 2.0f * kPi;
        SDL_RenderDrawLineF(renderer,
                            x + std::cos(t0) * radius,
                            y + std::sin(t0) * radius,
                            x + std::cos(t1) * radius,
                            y + std::sin(t1) * radius);
    }
}

class SensorBoxSession : public MapInteractionSession {
public:
    explicit SensorBoxSession(const nlohmann::json& payload) {
        if (payload.contains("minX") && payload.contains("minY") && payload.contains("maxX") && payload.contains("maxY")) {
            result_.payload = payload;
        }
    }

    void OnInput(const ToolContext& ctx, const InputEvent& event) override {
        if (event.type == InputEvent::Type::kKeyDown && event.key == Key::kEscape) {
            cancelled_ = true;
            return;
        }
        if (!ctx.viewport) {
            return;
        }
        switch (event.type) {
            case InputEvent::Type::kMouseDown:
                if (event.mouseButton == MouseButton::kLeft) {
                    auto world = ScreenToWorld(ctx, event);
                    start_ = {world.first, world.second};
                    current_ = start_;
                    dragging_ = true;
                }
                else if (event.mouseButton == MouseButton::kRight) {
                    cancelled_ = true;
                }
                break;
            case InputEvent::Type::kMouseMove:
                if (dragging_) {
                    auto world = ctx.viewport->ScreenToWorld(static_cast<float>(event.x), static_cast<float>(event.y));
                    current_ = SDL_FPoint{world.first, world.second};
                }
                break;
            case InputEvent::Type::kMouseUp:
                if (dragging_ && event.mouseButton == MouseButton::kLeft) {
                    dragging_ = false;
                    finished_ = true;
                    nlohmann::json payload;
                    payload["minX"] = std::min(start_.x, current_.x);
                    payload["minY"] = std::min(start_.y, current_.y);
                    payload["maxX"] = std::max(start_.x, current_.x);
                    payload["maxY"] = std::max(start_.y, current_.y);
                    result_.payload = payload;
                }
                break;
            default:
                break;
        }
    }

    void OnRender(const ToolContext& ctx, const RenderContext& context) override {
        if (!ctx.viewport || (!dragging_ && !finished_)) {
            return;
        }
        SDL_FPoint a = dragging_ ? start_ : SDL_FPoint{result_.payload.value("minX", 0.0f), result_.payload.value("minY", 0.0f)};
        SDL_FPoint b =
            dragging_ ? current_ : SDL_FPoint{result_.payload.value("maxX", 0.0f), result_.payload.value("maxY", 0.0f)};
        auto topLeft = ctx.viewport->WorldToScreen(std::min(a.x, b.x), std::min(a.y, b.y));
        auto bottomRight = ctx.viewport->WorldToScreen(std::max(a.x, b.x), std::max(a.y, b.y));
        SDL_Rect rect{static_cast<int>(topLeft.first),
                      static_cast<int>(topLeft.second),
                      static_cast<int>(bottomRight.first - topLeft.first),
                      static_cast<int>(bottomRight.second - topLeft.second)};
        SDL_SetRenderDrawColor(context.renderer, 200, 200, 80, 255);
        SDL_RenderDrawRect(context.renderer, &rect);
    }

    bool IsFinished() const override { return finished_; }
    bool IsCancelled() const override { return cancelled_; }
    MapInteractionResult BuildResult() const override { return result_; }

private:
    bool dragging_{false};
    bool finished_{false};
    bool cancelled_{false};
    SDL_FPoint start_{0.0f, 0.0f};
    SDL_FPoint current_{0.0f, 0.0f};
    MapInteractionResult result_{MapInteractionType::kSensorBox, nlohmann::json::object()};
};

struct PathPoint {
    SDL_FPoint position{0.0f, 0.0f};
    bool isStop{false};
};

class RailPathSession : public MapInteractionSession {
public:
    explicit RailPathSession(const nlohmann::json& payload) {
        if (payload.is_array()) {
            for (const auto& entry : payload) {
                PathPoint pt;
                pt.position.x = entry.value("x", 0.0f);
                pt.position.y = entry.value("y", 0.0f);
                pt.isStop = entry.value("isStop", false);
                points_.push_back(pt);
            }
        }
    }

    void OnInput(const ToolContext& ctx, const InputEvent& event) override {
        if (!ctx.viewport) {
            return;
        }
        switch (event.type) {
            case InputEvent::Type::kMouseDown:
                if (event.mouseButton == MouseButton::kLeft) {
                    auto world = ScreenToWorld(ctx, event);
                    int hitIndex = HitTest(ctx, world.first, world.second);
                    if (hitIndex >= 0) {
                        draggingIndex_ = hitIndex;
                    } else {
                        PathPoint pt;
                        pt.position = SDL_FPoint{world.first, world.second};
                        points_.push_back(pt);
                        draggingIndex_ = static_cast<int>(points_.size()) - 1;
                    }
                } else if (event.mouseButton == MouseButton::kRight) {
                    auto world = ScreenToWorld(ctx, event);
                    int hitIndex = HitTest(ctx, world.first, world.second);
                    if (hitIndex >= 0 && hitIndex < static_cast<int>(points_.size())) {
                        points_.erase(points_.begin() + hitIndex);
                    } else if (!points_.empty()) {
                        finished_ = true;
                    }
                }
                break;
            case InputEvent::Type::kMouseMove:
                if (draggingIndex_ >= 0) {
                    auto world = ScreenToWorld(ctx, event);
                    points_[draggingIndex_].position = SDL_FPoint{world.first, world.second};
                }
                break;
            case InputEvent::Type::kMouseUp:
                if (event.mouseButton == MouseButton::kLeft) {
                    draggingIndex_ = -1;
                }
                break;
            case InputEvent::Type::kKeyDown:
                if (event.key == Key::kEscape) {
                    cancelled_ = true;
                }
                break;
            default:
                break;
        }
    }

    void OnRender(const ToolContext& ctx, const RenderContext& context) override {
        if (!ctx.viewport || points_.empty()) {
            return;
        }
        SDL_SetRenderDrawColor(context.renderer, 80, 200, 220, 255);
        for (size_t i = 1; i < points_.size(); ++i) {
            const auto prev = ctx.viewport->WorldToScreen(points_[i - 1].position.x, points_[i - 1].position.y);
            const auto curr = ctx.viewport->WorldToScreen(points_[i].position.x, points_[i].position.y);
            SDL_RenderDrawLineF(context.renderer, prev.first, prev.second, curr.first, curr.second);
        }
        for (size_t i = 0; i < points_.size(); ++i) {
            const auto screen = ctx.viewport->WorldToScreen(points_[i].position.x, points_[i].position.y);
            SDL_SetRenderDrawColor(context.renderer, points_[i].isStop ? 255 : 200, 120, 60, 255);
            DrawCircle(context.renderer, screen.first, screen.second, 6.0f);
        }
    }

    bool IsFinished() const override { return finished_; }
    bool IsCancelled() const override { return cancelled_; }

    MapInteractionResult BuildResult() const override {
        MapInteractionResult result{MapInteractionType::kRailPath, nlohmann::json::array()};
        for (const auto& point : points_) {
            nlohmann::json entry;
            entry["x"] = point.position.x;
            entry["y"] = point.position.y;
            entry["isStop"] = point.isStop;
            result.payload.push_back(entry);
        }
        return result;
    }

private:
    int HitTest(const ToolContext& ctx, float worldX, float worldY) const {
        for (int i = static_cast<int>(points_.size()) - 1; i >= 0; --i) {
            if (IsWithinRadius(ctx, points_[i].position, worldX, worldY)) {
                return i;
            }
        }
        return -1;
    }

    std::vector<PathPoint> points_;
    int draggingIndex_{-1};
    bool finished_{false};
    bool cancelled_{false};
};

class JointAnchorSession : public MapInteractionSession {
public:
    explicit JointAnchorSession(const nlohmann::json& payload) {
        if (payload.contains("connectedBody") && payload["connectedBody"].is_string()) {
            connectedBody_ = payload["connectedBody"].get<std::string>();
            phase_ = Phase::kAnchorA;
        }
        if (payload.contains("anchorA") && payload["anchorA"].is_array() && payload["anchorA"].size() == 2) {
            anchorA_ = SDL_FPoint{payload["anchorA"][0].get<float>(), payload["anchorA"][1].get<float>()};
            phase_ = Phase::kAnchorB;
        }
        if (payload.contains("anchorB") && payload["anchorB"].is_array() && payload["anchorB"].size() == 2) {
            anchorB_ = SDL_FPoint{payload["anchorB"][0].get<float>(), payload["anchorB"][1].get<float>()};
        }
    }

    void OnInput(const ToolContext& ctx, const InputEvent& event) override {
        if (!ctx.viewport) {
            return;
        }
        if (event.type == InputEvent::Type::kKeyDown && event.key == Key::kEscape) {
            cancelled_ = true;
            return;
        }
        if (event.type == InputEvent::Type::kMouseDown && event.mouseButton == MouseButton::kLeft) {
            auto world = ScreenToWorld(ctx, event);
            switch (phase_) {
                case Phase::kSelectBody: {
                    const std::string pick =
                        tool_utils::PickObject(ctx.levelState, world.first, world.second, nullptr, nullptr);
                    if (!pick.empty()) {
                        connectedBody_ = pick;
                        phase_ = Phase::kAnchorA;
                    }
                    break;
                }
                case Phase::kAnchorA:
                    anchorA_ = SDL_FPoint{world.first, world.second};
                    phase_ = Phase::kAnchorB;
                    break;
                case Phase::kAnchorB:
                    anchorB_ = SDL_FPoint{world.first, world.second};
                    finished_ = true;
                    break;
            }
        }
    }

    void OnRender(const ToolContext& ctx, const RenderContext& context) override {
        if (!ctx.viewport) {
            return;
        }
        SDL_SetRenderDrawColor(context.renderer, 255, 210, 64, 255);
        if (anchorA_.has_value()) {
            const auto screen = ctx.viewport->WorldToScreen(anchorA_->x, anchorA_->y);
            DrawCircle(context.renderer, screen.first, screen.second, 6.0f);
        }
        if (anchorB_.has_value()) {
            const auto screen = ctx.viewport->WorldToScreen(anchorB_->x, anchorB_->y);
            DrawCircle(context.renderer, screen.first, screen.second, 6.0f);
        }
    }

    bool IsFinished() const override { return finished_; }
    bool IsCancelled() const override { return cancelled_; }

    MapInteractionResult BuildResult() const override {
        MapInteractionResult result{MapInteractionType::kJointAnchors, nlohmann::json::object()};
        result.payload["connectedBody"] = connectedBody_;
        if (anchorA_) {
            result.payload["anchorA"] = {anchorA_->x, anchorA_->y};
        }
        if (anchorB_) {
            result.payload["anchorB"] = {anchorB_->x, anchorB_->y};
        }
        return result;
    }

private:
    enum class Phase { kSelectBody, kAnchorA, kAnchorB };

    Phase phase_{Phase::kSelectBody};
    std::string connectedBody_;
    std::optional<SDL_FPoint> anchorA_;
    std::optional<SDL_FPoint> anchorB_;
    bool finished_{false};
    bool cancelled_{false};
};

class SpawnerLocationSession : public MapInteractionSession {
public:
    explicit SpawnerLocationSession(const nlohmann::json& payload) {
        if (payload.is_array()) {
            for (const auto& entry : payload) {
                SDL_FPoint pt;
                pt.x = entry.value("x", 0.0f);
                pt.y = entry.value("y", 0.0f);
                points_.push_back(pt);
            }
        }
    }

    void OnInput(const ToolContext& ctx, const InputEvent& event) override {
        if (!ctx.viewport) {
            return;
        }
        switch (event.type) {
            case InputEvent::Type::kMouseDown:
                if (event.mouseButton == MouseButton::kLeft) {
                    auto world = ScreenToWorld(ctx, event);
                    int hit = HitTest(ctx, world.first, world.second);
                    if (hit >= 0) {
                        draggingIndex_ = hit;
                    } else {
                        points_.push_back(SDL_FPoint{world.first, world.second});
                        draggingIndex_ = static_cast<int>(points_.size()) - 1;
                    }
                } else if (event.mouseButton == MouseButton::kRight) {
                    auto world = ScreenToWorld(ctx, event);
                    int hit = HitTest(ctx, world.first, world.second);
                    if (hit >= 0 && hit < static_cast<int>(points_.size())) {
                        points_.erase(points_.begin() + hit);
                        if (draggingIndex_ == hit) {
                            draggingIndex_ = -1;
                        } else if (draggingIndex_ > hit) {
                            --draggingIndex_;
                        }
                    } else {
                        cancelled_ = true;
                    }
                }
                break;
            case InputEvent::Type::kMouseMove:
                if (draggingIndex_ >= 0) {
                    auto world = ScreenToWorld(ctx, event);
                    points_[draggingIndex_] = SDL_FPoint{world.first, world.second};
                }
                break;
            case InputEvent::Type::kMouseUp:
                if (event.mouseButton == MouseButton::kLeft) {
                    draggingIndex_ = -1;
                }
                break;
            case InputEvent::Type::kKeyDown:
                if (event.key == Key::kEscape) {
                    cancelled_ = true;
                }
                break;
            default:
                break;
        }
    }

    void OnRender(const ToolContext& ctx, const RenderContext& context) override {
        if (!ctx.viewport) {
            return;
        }
        SDL_SetRenderDrawColor(context.renderer, 120, 220, 140, 255);
        for (const auto& point : points_) {
            const auto screen = ctx.viewport->WorldToScreen(point.x, point.y);
            DrawCircle(context.renderer, screen.first, screen.second, 5.0f);
        }
    }

    bool IsFinished() const override { return finished_; }
    bool IsCancelled() const override { return cancelled_; }

    MapInteractionResult BuildResult() const override {
        MapInteractionResult result{MapInteractionType::kSpawnerLocations, nlohmann::json::array()};
        for (const auto& pt : points_) {
            nlohmann::json entry;
            entry["x"] = pt.x;
            entry["y"] = pt.y;
            result.payload.push_back(entry);
        }
        return result;
    }

private:
    int HitTest(const ToolContext& ctx, float worldX, float worldY) const {
        for (int i = static_cast<int>(points_.size()) - 1; i >= 0; --i) {
            if (IsWithinRadius(ctx, points_[i], worldX, worldY)) {
                return i;
            }
        }
        return -1;
    }

    std::vector<SDL_FPoint> points_;
    int draggingIndex_{-1};
    bool finished_{false};
    bool cancelled_{false};
};

}  // namespace

std::unique_ptr<MapInteractionSession> CreateMapInteractionSession(MapInteractionType type, const nlohmann::json& payload) {
    switch (type) {
        case MapInteractionType::kSensorBox:
            return std::make_unique<SensorBoxSession>(payload);
        case MapInteractionType::kRailPath:
            return std::make_unique<RailPathSession>(payload);
        case MapInteractionType::kJointAnchors:
            return std::make_unique<JointAnchorSession>(payload);
        case MapInteractionType::kSpawnerLocations:
            return std::make_unique<SpawnerLocationSession>(payload);
    }
    return nullptr;
}

}  // namespace level_editor


