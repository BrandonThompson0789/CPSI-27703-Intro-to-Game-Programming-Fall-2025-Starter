#include "ViewGrabComponent.h"

#include "ComponentLibrary.h"
#include "BodyComponent.h"
#include "SpriteComponent.h"

#include "../Engine.h"
#include "../Object.h"

#include <algorithm>
#include <tuple>

namespace {
constexpr float DEFAULT_PADDING_X = 200.0f;
constexpr float DEFAULT_PADDING_Y = 140.0f;
constexpr float DEFAULT_MIN_HALF_EXTENT = 24.0f;
} // namespace

ViewGrabComponent::ViewGrabComponent(Object& parent)
    : Component(parent),
      paddingX(DEFAULT_PADDING_X),
      paddingY(DEFAULT_PADDING_Y),
      minHalfExtent(DEFAULT_MIN_HALF_EXTENT),
      offsetX(0.0f),
      offsetY(0.0f) {
    ++activeComponents;
}

ViewGrabComponent::ViewGrabComponent(Object& parent, const nlohmann::json& data)
    : Component(parent),
      paddingX(data.value("paddingX", DEFAULT_PADDING_X)),
      paddingY(data.value("paddingY", DEFAULT_PADDING_Y)),
      minHalfExtent(data.value("minHalfExtent", DEFAULT_MIN_HALF_EXTENT)),
      offsetX(data.value("offsetX", 0.0f)),
      offsetY(data.value("offsetY", 0.0f)) {
    ++activeComponents;
}

ViewGrabComponent::~ViewGrabComponent() {
    activeComponents = std::max(0, activeComponents - 1);
}

void ViewGrabComponent::beginFrame() {
    frameMinX = std::numeric_limits<float>::infinity();
    frameMinY = std::numeric_limits<float>::infinity();
    frameMaxX = -std::numeric_limits<float>::infinity();
    frameMaxY = -std::numeric_limits<float>::infinity();
    frameHasBounds = false;
}

void ViewGrabComponent::finalizeFrame(Engine& engine) {
    if (!frameHasBounds) {
        engine.ensureDefaultCamera();
        return;
    }

    engine.applyViewBounds(frameMinX, frameMinY, frameMaxX, frameMaxY);
}

void ViewGrabComponent::update(float /*deltaTime*/) {
    accumulateBounds();
}

void ViewGrabComponent::draw() {
    // This component only influences the camera; no direct drawing.
}

nlohmann::json ViewGrabComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["paddingX"] = paddingX;
    data["paddingY"] = paddingY;
    data["minHalfExtent"] = minHalfExtent;
    if (offsetX != 0.0f) {
        data["offsetX"] = offsetX;
    }
    if (offsetY != 0.0f) {
        data["offsetY"] = offsetY;
    }
    return data;
}

void ViewGrabComponent::accumulateBounds() {
    if (!Object::getEngine()) {
        return;
    }

    float posX = 0.0f;
    float posY = 0.0f;
    float halfWidth = minHalfExtent;
    float halfHeight = minHalfExtent;

    if (auto* body = parent().getComponent<BodyComponent>()) {
        float angle = 0.0f;
        std::tie(posX, posY, angle) = body->getPosition();
        (void)angle;

        auto [fixtureWidth, fixtureHeight] = body->getFixtureSize();
        if (fixtureWidth > 0.0f) {
            halfWidth = std::max(halfWidth, fixtureWidth * 0.5f);
        }
        if (fixtureHeight > 0.0f) {
            halfHeight = std::max(halfHeight, fixtureHeight * 0.5f);
        }
    } else if (auto* sprite = parent().getComponent<SpriteComponent>()) {
        float angle = 0.0f;
        std::tie(posX, posY, angle) = sprite->getPosition();
        (void)angle;
    } else {
        return;
    }

    const float focusX = posX + offsetX;
    const float focusY = posY + offsetY;

    const float paddedMinX = posX - halfWidth - paddingX;
    const float paddedMaxX = posX + halfWidth + paddingX;
    const float paddedMinY = posY - halfHeight - paddingY;
    const float paddedMaxY = posY + halfHeight + paddingY;

    frameMinX = std::min({frameMinX, paddedMinX, focusX});
    frameMinY = std::min({frameMinY, paddedMinY, focusY});
    frameMaxX = std::max({frameMaxX, paddedMaxX, focusX});
    frameMaxY = std::max({frameMaxY, paddedMaxY, focusY});

    frameHasBounds = true;
}

// Register component
static ComponentRegistrar<ViewGrabComponent> registrar("ViewGrabComponent");


