#pragma once

#include "Component.h"

#include <limits>

class Engine;

class ViewGrabComponent : public Component {
public:
    explicit ViewGrabComponent(Object& parent);
    ViewGrabComponent(Object& parent, const nlohmann::json& data);
    ~ViewGrabComponent() override;

    void update(float deltaTime) override;
    void draw() override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "ViewGrabComponent"; }

    static void beginFrame();
    static void finalizeFrame(Engine& engine);

private:
    void accumulateBounds();

    float paddingX;
    float paddingY;
    float minHalfExtent;
    float offsetX;
    float offsetY;

    static inline float frameMinX = std::numeric_limits<float>::infinity();
    static inline float frameMinY = std::numeric_limits<float>::infinity();
    static inline float frameMaxX = -std::numeric_limits<float>::infinity();
    static inline float frameMaxY = -std::numeric_limits<float>::infinity();
    static inline bool frameHasBounds = false;
    static inline int activeComponents = 0;
};


