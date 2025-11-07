#include "Box2DDebugDraw.h"

#include <cmath>
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
    : renderer(nullptr), pixelsPerMeter(1.0f), enabled(false), initialized(false), debugDraw{} {}

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
    float half = size * pixelsPerMeter * 0.5f;
    SDL_Rect rect;
    rect.x = static_cast<int>(center.x - half);
    rect.y = static_cast<int>(center.y - half);
    rect.w = static_cast<int>(size * pixelsPerMeter);
    rect.h = rect.w;
    SDL_RenderDrawRect(renderer, &rect);
}

SDL_FPoint Box2DDebugDraw::toScreen(b2Vec2 position) const {
    return {position.x * pixelsPerMeter, position.y * pixelsPerMeter};
}

void Box2DDebugDraw::useColor(b2HexColor color) {
    SDL_SetRenderDrawColor(renderer, extractRed(color), extractGreen(color), extractBlue(color), 255);
}

