#include "Box2DDebugDraw.h"

#include "Object.h"
#include "Engine.h"
#include "components/BodyComponent.h"
#include "components/HealthComponent.h"
#include "components/SensorComponent.h"
#include "components/RailComponent.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
constexpr int kCircleSegments = 16;
constexpr float kPi = 3.14159265358979323846f;

Uint8 extractRed(b2HexColor color) {
    return static_cast<Uint8>((color >> 16) & 0xFF);
}

Uint8 extractGreen(b2HexColor color) {
    return static_cast<Uint8>((color >> 8) & 0xFF);
}

Uint8 extractBlue(b2HexColor color) {
    return static_cast<Uint8>(color & 0xFF);
}
} // namespace

Box2DDebugDraw::Box2DDebugDraw()
    : renderer(nullptr),
      pixelsPerMeter(1.0f),
      enabled(false),
      initialized(false),
      debugDraw{},
      cameraScale(1.0f),
      cameraOriginX(0.0f),
      cameraOriginY(0.0f),
      fontPath("assets/fonts/SuperPixel-m2L8j.ttf"),
      fontSize(14),
      labelFont(nullptr) {}

Box2DDebugDraw::~Box2DDebugDraw() {
    shutdown();
}

void Box2DDebugDraw::init(SDL_Renderer* inRenderer, float inPixelsPerMeter) {
    if (!inRenderer) {
        return;
    }

    renderer = inRenderer;
    pixelsPerMeter = inPixelsPerMeter;
    debugDraw = b2DefaultDebugDraw();

    debugDraw.DrawPolygonFcn = &Box2DDebugDraw::DrawPolygon;
    debugDraw.DrawSolidPolygonFcn = &Box2DDebugDraw::DrawSolidPolygon;
    debugDraw.DrawCircleFcn = &Box2DDebugDraw::DrawCircle;
    debugDraw.DrawSolidCircleFcn = &Box2DDebugDraw::DrawSolidCircle;
    debugDraw.DrawSolidCapsuleFcn = &Box2DDebugDraw::DrawSolidCapsule;
    debugDraw.DrawSegmentFcn = &Box2DDebugDraw::DrawSegment;
    debugDraw.DrawTransformFcn = &Box2DDebugDraw::DrawTransform;
    debugDraw.DrawPointFcn = &Box2DDebugDraw::DrawPoint;
    debugDraw.context = this;

    debugDraw.drawShapes = true;
    debugDraw.drawJoints = true;
    debugDraw.drawBounds = false;
    debugDraw.drawContacts = false;

    initialized = true;
}

void Box2DDebugDraw::shutdown() {
    if (labelFont) {
        TTF_CloseFont(labelFont);
        labelFont = nullptr;
    }
    renderer = nullptr;
    initialized = false;
    enabled = false;
    debugDraw = b2DefaultDebugDraw();
    debugDraw.context = this;
}

void Box2DDebugDraw::setEnabled(bool value) {
    enabled = value && initialized;
}

bool Box2DDebugDraw::isEnabled() const {
    return enabled;
}

void Box2DDebugDraw::toggle() {
    if (!initialized) {
        return;
    }
    enabled = !enabled;
}

b2DebugDraw* Box2DDebugDraw::getInterface() {
    return initialized ? &debugDraw : nullptr;
}

bool Box2DDebugDraw::setLabelFont(const std::string& path, int pointSize) {
    fontPath = path;
    fontSize = pointSize;

    if (labelFont) {
        TTF_CloseFont(labelFont);
        labelFont = nullptr;
    }

    if (!TTF_WasInit()) {
        std::cerr << "SDL_ttf not initialized. Call TTF_Init() before setting fonts." << std::endl;
        return false;
    }

    labelFont = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!labelFont) {
        std::cerr << "Failed to load debug draw font '" << fontPath << "': " << TTF_GetError() << std::endl;
        return false;
    }

    return true;
}

void Box2DDebugDraw::setCamera(float scale, float viewMinX, float viewMinY) {
    cameraScale = scale > 0.0f ? scale : 1.0f;
    cameraOriginX = viewMinX;
    cameraOriginY = viewMinY;
}

void Box2DDebugDraw::DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawPolygonImpl(vertices, vertexCount, color);
}

void Box2DDebugDraw::DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float /*radius*/, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    std::vector<b2Vec2> transformed(static_cast<std::size_t>(vertexCount));
    for (int i = 0; i < vertexCount; ++i) {
        transformed[static_cast<std::size_t>(i)] = b2TransformPoint(transform, vertices[i]);
    }

    self->drawPolygonImpl(transformed.data(), vertexCount, color);
}

void Box2DDebugDraw::DrawCircle(b2Vec2 center, float radius, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawCircleImpl(center, radius, color);
}

void Box2DDebugDraw::DrawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawCircleImpl(transform.p, radius, color);

    // Draw an axis to show rotation
    b2Vec2 axis = {radius, 0.0f};
    b2Vec2 axisEnd = b2Add(transform.p, b2RotateVector(transform.q, axis));
    self->drawSegmentImpl(transform.p, axisEnd, color);
}

void Box2DDebugDraw::DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawCapsuleImpl(p1, p2, radius, color);
}

void Box2DDebugDraw::DrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawSegmentImpl(p1, p2, color);
}

void Box2DDebugDraw::DrawTransform(b2Transform transform, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawTransformImpl(transform);
}

void Box2DDebugDraw::DrawPoint(b2Vec2 p, float size, b2HexColor color, void* context) {
    auto* self = static_cast<Box2DDebugDraw*>(context);
    if (!self || !self->renderer) {
        return;
    }

    self->drawPointImpl(p, size, color);
}

void Box2DDebugDraw::drawPolygonImpl(const b2Vec2* vertices, int vertexCount, b2HexColor color) {
    if (vertexCount <= 1) {
        return;
    }

    useColor(color);

    std::vector<SDL_FPoint> points(static_cast<std::size_t>(vertexCount) + 1);
    for (int i = 0; i < vertexCount; ++i) {
        points[static_cast<std::size_t>(i)] = toScreen(vertices[i]);
    }
    points.back() = points.front();

    SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
}

void Box2DDebugDraw::drawCircleImpl(b2Vec2 center, float radius, b2HexColor color) {
    useColor(color);

    std::vector<SDL_FPoint> points(kCircleSegments + 1);
    for (int i = 0; i <= kCircleSegments; ++i) {
        float angle = static_cast<float>(i) / static_cast<float>(kCircleSegments) * 2.0f * kPi;
        b2Vec2 vertex = {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
        points[static_cast<std::size_t>(i)] = toScreen(vertex);
    }

    SDL_RenderDrawLinesF(renderer, points.data(), static_cast<int>(points.size()));
}

void Box2DDebugDraw::drawCapsuleImpl(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color) {
    // Approximate capsule with line plus circle caps
    drawSegmentImpl(p1, p2, color);
    drawCircleImpl(p1, radius, color);
    drawCircleImpl(p2, radius, color);
}

void Box2DDebugDraw::drawSegmentImpl(b2Vec2 p1, b2Vec2 p2, b2HexColor color) {
    useColor(color);

    SDL_FPoint start = toScreen(p1);
    SDL_FPoint end = toScreen(p2);
    SDL_RenderDrawLineF(renderer, start.x, start.y, end.x, end.y);
}

void Box2DDebugDraw::drawTransformImpl(b2Transform transform) {
    // X axis (red)
    useColor(b2_colorRed);
    SDL_FPoint origin = toScreen(transform.p);
    b2Vec2 xVec = {0.5f, 0.0f};
    b2Vec2 xAxis = b2Add(transform.p, b2RotateVector(transform.q, xVec));
    SDL_FPoint xPoint = toScreen(xAxis);
    SDL_RenderDrawLineF(renderer, origin.x, origin.y, xPoint.x, xPoint.y);

    // Y axis (green)
    useColor(b2_colorGreen);
    b2Vec2 yVec = {0.0f, 0.5f};
    b2Vec2 yAxis = b2Add(transform.p, b2RotateVector(transform.q, yVec));
    SDL_FPoint yPoint = toScreen(yAxis);
    SDL_RenderDrawLineF(renderer, origin.x, origin.y, yPoint.x, yPoint.y);
}

void Box2DDebugDraw::drawPointImpl(b2Vec2 p, float size, b2HexColor color) {
    useColor(color);
    SDL_FPoint center = toScreen(p);
    float sizePixels = size * pixelsPerMeter * cameraScale;
    float half = sizePixels * 0.5f;
    SDL_FRect rect;
    rect.x = center.x - half;
    rect.y = center.y - half;
    rect.w = sizePixels;
    rect.h = sizePixels;
    SDL_RenderDrawRectF(renderer, &rect);
}

SDL_FPoint Box2DDebugDraw::toScreen(b2Vec2 position) const {
    const float worldX = position.x * pixelsPerMeter;
    const float worldY = position.y * pixelsPerMeter;
    return {
        (worldX - cameraOriginX) * cameraScale,
        (worldY - cameraOriginY) * cameraScale
    };
}

void Box2DDebugDraw::useColor(b2HexColor color) {
    SDL_SetRenderDrawColor(renderer, extractRed(color), extractGreen(color), extractBlue(color), 255);
}

bool Box2DDebugDraw::ensureFontLoaded() {
    if (labelFont) {
        return true;
    }

    if (!TTF_WasInit()) {
        std::cerr << "SDL_ttf not initialized. Cannot load debug draw font." << std::endl;
        return false;
    }

    if (fontPath.empty()) {
        std::cerr << "No font path configured for Box2DDebugDraw labels." << std::endl;
        return false;
    }

    labelFont = TTF_OpenFont(fontPath.c_str(), fontSize);
    if (!labelFont) {
        std::cerr << "Failed to load debug draw font '" << fontPath << "': " << TTF_GetError() << std::endl;
        return false;
    }

    return true;
}

void Box2DDebugDraw::drawTextCentered(const std::string& text, float centerX, float y, const SDL_Color& color, int* textHeight) {
    if (!renderer || text.empty()) {
        return;
    }

    if (!ensureFontLoaded()) {
        return;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(labelFont, text.c_str(), color);
    if (!surface) {
        std::cerr << "Failed to render debug draw text '" << text << "': " << TTF_GetError() << std::endl;
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "Failed to create texture for debug draw text '" << text << "': " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect destination;
    destination.w = surface->w;
    destination.h = surface->h;
    destination.x = static_cast<int>(std::round(centerX - static_cast<float>(destination.w) * 0.5f));
    destination.y = static_cast<int>(std::round(y));

    if (textHeight) {
        *textHeight = destination.h;
    }

    SDL_RenderCopy(renderer, texture, nullptr, &destination);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void Box2DDebugDraw::renderLabels(const std::vector<std::unique_ptr<Object>>& objects) {
    if (!enabled || !renderer || !ensureFontLoaded()) {
        return;
    }

    Uint8 prevR = 0;
    Uint8 prevG = 0;
    Uint8 prevB = 0;
    Uint8 prevA = 0;
    SDL_GetRenderDrawColor(renderer, &prevR, &prevG, &prevB, &prevA);

    const SDL_Color nameColor{255, 255, 255, 255};
    const SDL_Color healthTextColor{255, 255, 255, 255};
    const SDL_Color healthBgColor{30, 30, 30, 230};
    const SDL_Color healthBorderColor{220, 220, 220, 255};
    const SDL_Color healthGoodColor{80, 150, 100, 255};
    const SDL_Color healthMidColor{200, 180, 0, 255};
    const SDL_Color healthLowColor{220, 80, 60, 255};
    const SDL_Color sensorTextColor{200, 240, 255, 255};

    // First pass: Draw sensor range circles, target lines, and rail paths (before labels)
    for (const auto& objectPtr : objects) {
        if (!objectPtr) {
            continue;
        }

        auto* sensor = objectPtr->getComponent<SensorComponent>();
        if (!sensor) {
            continue;
        }

        auto* body = objectPtr->getComponent<BodyComponent>();
        if (!body) {
            continue;
        }

        auto [posX, posY, angleDegrees] = body->getPosition();
        (void)angleDegrees;

        // Convert from pixels to meters for Box2D world coordinates
        b2Vec2 sensorPos{posX * Engine::PIXELS_TO_METERS, posY * Engine::PIXELS_TO_METERS};

        // Draw range circle if sensor has distance condition
        if (sensor->hasDistanceCondition()) {
            float maxDistance = sensor->getMaxDistance();
            // maxDistance is in pixels, convert to meters
            // Light blue color: 0x64C8FF (RGB: 100, 200, 255)
            drawCircleImpl(sensorPos, maxDistance * Engine::PIXELS_TO_METERS, static_cast<b2HexColor>(0x64C8FF));
        }

        // Draw lines to target objects
        std::vector<Object*> targetObjects = sensor->getTargetObjects(objects);
        for (Object* target : targetObjects) {
            if (!target || !Object::isAlive(target)) {
                continue;
            }
            auto* targetBody = target->getComponent<BodyComponent>();
            if (!targetBody) {
                continue;
            }
            auto [targetX, targetY, targetAngle] = targetBody->getPosition();
            (void)targetAngle;
            // Convert from pixels to meters
            b2Vec2 targetPos{targetX * Engine::PIXELS_TO_METERS, targetY * Engine::PIXELS_TO_METERS};
            // Orange color: 0xFFC864 (RGB: 255, 200, 100)
            drawSegmentImpl(sensorPos, targetPos, static_cast<b2HexColor>(0xFFC864));
        }
    }

    // Draw rail component paths
    for (const auto& objectPtr : objects) {
        if (!objectPtr) {
            continue;
        }

        auto* rail = objectPtr->getComponent<RailComponent>();
        if (!rail) {
            continue;
        }

        const auto& path = rail->getPath();
        if (path.size() < 2) {
            continue;
        }

        // Draw rail path segments
        // Green color for rail path: 0x00FF00 (RGB: 0, 255, 0)
        // Yellow color for stop points: 0xFFFF00 (RGB: 255, 255, 0)
        b2HexColor pathColor = static_cast<b2HexColor>(0x00FF00);
        b2HexColor stopColor = static_cast<b2HexColor>(0xFFFF00);

        // Draw lines connecting all points in the path
        for (size_t i = 0; i < path.size(); ++i) {
            size_t nextIdx = (i + 1) % path.size();
            const auto& point1 = path[i];
            const auto& point2 = path[nextIdx];
            
            // Convert from pixels to meters
            b2Vec2 p1{point1.x * Engine::PIXELS_TO_METERS, point1.y * Engine::PIXELS_TO_METERS};
            b2Vec2 p2{point2.x * Engine::PIXELS_TO_METERS, point2.y * Engine::PIXELS_TO_METERS};
            
            drawSegmentImpl(p1, p2, pathColor);
        }

        // Draw stop points as circles
        for (const auto& point : path) {
            if (point.isStop) {
                b2Vec2 pos{point.x * Engine::PIXELS_TO_METERS, point.y * Engine::PIXELS_TO_METERS};
                // Draw a small circle at stop points (radius of 0.1 meters = 10 pixels)
                drawCircleImpl(pos, 0.1f, stopColor);
            }
        }

        // Draw current target if moving
        if (rail->isMoving()) {
            int targetIdx = rail->getCurrentTargetIndex();
            if (targetIdx >= 0 && targetIdx < static_cast<int>(path.size())) {
                const auto& target = path[targetIdx];
                b2Vec2 targetPos{target.x * Engine::PIXELS_TO_METERS, target.y * Engine::PIXELS_TO_METERS};
                // Draw a larger circle at current target (radius of 0.15 meters = 15 pixels)
                // Cyan color: 0x00FFFF (RGB: 0, 255, 255)
                drawCircleImpl(targetPos, 0.15f, static_cast<b2HexColor>(0x00FFFF));
            }
        }
    }

    // Second pass: Draw labels (health, name, sensor info)
    for (const auto& objectPtr : objects) {
        if (!objectPtr) {
            continue;
        }

        auto* body = objectPtr->getComponent<BodyComponent>();
        if (!body) {
            continue;
        }

        auto [posX, posY, angleDegrees] = body->getPosition();
        (void)angleDegrees;

        auto [fixtureWidth, fixtureHeight] = body->getFixtureSize();
        if (fixtureWidth <= 0.0f) {
            fixtureWidth = 32.0f;
        }
        if (fixtureHeight <= 0.0f) {
            fixtureHeight = 32.0f;
        }

        const float scale = cameraScale > 0.0f ? cameraScale : 1.0f;
        SDL_FPoint screenCenter{
            (posX - cameraOriginX) * scale,
            (posY - cameraOriginY) * scale
        };
        float screenFixtureWidth = std::max(fixtureWidth * scale, 4.0f);
        float screenFixtureHeight = std::max(fixtureHeight * scale, 4.0f);
        float spacing = std::max(6.0f * scale, 3.0f);

        float objectTopY = screenCenter.y - (screenFixtureHeight * 0.5f);
        float currentLabelTop = objectTopY - spacing;

        auto* health = objectPtr->getComponent<HealthComponent>();
        if (health && health->getMaxHP() > 0.0f) {
            float maxHP = health->getMaxHP();
            float currentHP = std::clamp(health->getCurrentHP(), 0.0f, maxHP);
            float normalized = std::clamp(currentHP / maxHP, 0.0f, 1.0f);

            float barWidthWorld = std::max(fixtureWidth, 60.0f);
            float barHeightWorld = 15.0f;

            int hpTextWidth = 0;
            int hpTextHeight = 0;
            std::ostringstream hpLabel;
            hpLabel << static_cast<int>(std::round(currentHP)) << " / " << static_cast<int>(std::round(maxHP));
            bool sizeOk = (TTF_SizeUTF8(labelFont, hpLabel.str().c_str(), &hpTextWidth, &hpTextHeight) == 0);
            if (sizeOk) {
                barHeightWorld = std::max(barHeightWorld, static_cast<float>(hpTextHeight) + 4.0f);
            }

            float barWidth = std::max(barWidthWorld * scale, 45.0f);
            float barHeight = std::max(barHeightWorld * scale, 6.0f);
            float barTop = currentLabelTop - barHeight;
            float barLeft = screenCenter.x - (barWidth * 0.5f);

            SDL_FRect bgRect{barLeft, barTop, barWidth, barHeight};
            SDL_SetRenderDrawColor(renderer, healthBgColor.r, healthBgColor.g, healthBgColor.b, healthBgColor.a);
            SDL_RenderFillRectF(renderer, &bgRect);

            SDL_FRect fillRect{barLeft, barTop, barWidth * normalized, barHeight};
            const SDL_Color& fillColor = normalized > 0.66f
                                             ? healthGoodColor
                                             : (normalized > 0.33f ? healthMidColor : healthLowColor);
            SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
            SDL_RenderFillRectF(renderer, &fillRect);

            SDL_SetRenderDrawColor(renderer, healthBorderColor.r, healthBorderColor.g, healthBorderColor.b, healthBorderColor.a);
            SDL_RenderDrawRectF(renderer, &bgRect);

            if (sizeOk) {
                float textY = barTop + (barHeight - static_cast<float>(hpTextHeight)) * 0.5f;
                drawTextCentered(hpLabel.str(), screenCenter.x, textY, healthTextColor);
            } else {
                drawTextCentered(hpLabel.str(), screenCenter.x, barTop, healthTextColor);
            }

            currentLabelTop = barTop - spacing;
        }

        // Draw sensor information
        auto* sensor = objectPtr->getComponent<SensorComponent>();
        if (sensor) {
            int conditionCount = sensor->getConditionCount();
            
            // Convert allObjects vector to Object* vector for getSatisfiedConditionCount
            std::vector<Object*> allObjectPtrs;
            allObjectPtrs.reserve(objects.size());
            for (const auto& obj : objects) {
                if (obj && Object::isAlive(obj.get())) {
                    allObjectPtrs.push_back(obj.get());
                }
            }
            int satisfiedCount = sensor->getSatisfiedConditionCount(allObjectPtrs);

            std::ostringstream sensorLabel;
            sensorLabel << "Sensor: " << satisfiedCount << "/" << conditionCount;
            if (sensor->hasDistanceCondition()) {
                sensorLabel << " (range: " << static_cast<int>(std::round(sensor->getMaxDistance())) << ")";
            }

            int sensorTextWidth = 0;
            int sensorTextHeight = 0;
            if (TTF_SizeUTF8(labelFont, sensorLabel.str().c_str(), &sensorTextWidth, &sensorTextHeight) != 0) {
                sensorTextHeight = fontSize;
            }

            float sensorTop = currentLabelTop - static_cast<float>(sensorTextHeight);
            drawTextCentered(sensorLabel.str(), screenCenter.x, sensorTop, sensorTextColor);
            currentLabelTop = sensorTop - spacing;
        }

        const std::string& name = objectPtr->getName();
        if (!name.empty()) {
            int nameTextWidth = 0;
            int nameTextHeight = 0;
            if (TTF_SizeUTF8(labelFont, name.c_str(), &nameTextWidth, &nameTextHeight) != 0) {
                nameTextHeight = fontSize;
            }

            float nameTop = currentLabelTop - static_cast<float>(nameTextHeight);
            drawTextCentered(name, screenCenter.x, nameTop, nameColor);
            currentLabelTop = nameTop - spacing;
        }
    }

    SDL_SetRenderDrawColor(renderer, prevR, prevG, prevB, prevA);
}

