#ifndef BOX2DDEBUGDRAW_H
#define BOX2DDEBUGDRAW_H

#include <SDL.h>
#include <SDL_ttf.h>
#include <box2d/box2d.h>

#include <memory>
#include <string>
#include <vector>

class Object;

class Box2DDebugDraw {
public:
    Box2DDebugDraw();
    ~Box2DDebugDraw();

    void init(SDL_Renderer* renderer, float pixelsPerMeter);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void toggle();

    b2DebugDraw* getInterface();
    bool setLabelFont(const std::string& path, int pointSize);
    void renderLabels(const std::vector<std::unique_ptr<Object>>& objects);
    void setCamera(float scale, float viewMinX, float viewMinY);

private:
    static void DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context);
    static void DrawSolidPolygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color, void* context);
    static void DrawCircle(b2Vec2 center, float radius, b2HexColor color, void* context);
    static void DrawSolidCircle(b2Transform transform, float radius, b2HexColor color, void* context);
    static void DrawSolidCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context);
    static void DrawSegment(b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context);
    static void DrawTransform(b2Transform transform, void* context);
    static void DrawPoint(b2Vec2 p, float size, b2HexColor color, void* context);

    void drawPolygonImpl(const b2Vec2* vertices, int vertexCount, b2HexColor color);
    void drawCircleImpl(b2Vec2 center, float radius, b2HexColor color);
    void drawCapsuleImpl(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color);
    void drawSegmentImpl(b2Vec2 p1, b2Vec2 p2, b2HexColor color);
    void drawTransformImpl(b2Transform transform);
    void drawPointImpl(b2Vec2 p, float size, b2HexColor color);

    SDL_FPoint toScreen(b2Vec2 position) const;
    void useColor(b2HexColor color);
    bool ensureFontLoaded();
    void drawTextCentered(const std::string& text, float centerX, float y, const SDL_Color& color, int* textHeight = nullptr);

    SDL_Renderer* renderer;
    float pixelsPerMeter;
    bool enabled;
    bool initialized;
    b2DebugDraw debugDraw;
    float cameraScale;
    float cameraOriginX;
    float cameraOriginY;
    std::string fontPath;
    int fontSize;
    TTF_Font* labelFont;
};

#endif // BOX2DDEBUGDRAW_H

