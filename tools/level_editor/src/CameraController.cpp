#include "level_editor/CameraController.h"

#include <algorithm>

namespace level_editor {

CameraController::CameraController() = default;

void CameraController::Update(const CameraInputState& inputState, float deltaSeconds) {
    float deltaX = 0.0f;
    float deltaY = 0.0f;

    if (inputState.moveLeft) {
        deltaX -= panSpeed_ * deltaSeconds;
    }
    if (inputState.moveRight) {
        deltaX += panSpeed_ * deltaSeconds;
    }
    if (inputState.moveUp) {
        deltaY -= panSpeed_ * deltaSeconds;
    }
    if (inputState.moveDown) {
        deltaY += panSpeed_ * deltaSeconds;
    }

    camera_.positionX += deltaX / camera_.zoom;
    camera_.positionY += deltaY / camera_.zoom;
}

void CameraController::BeginDrag(int screenX, int screenY) {
    dragging_ = true;
    lastDragX_ = screenX;
    lastDragY_ = screenY;
}

void CameraController::DragTo(int screenX, int screenY) {
    if (!dragging_) {
        return;
    }
    const int deltaX = screenX - lastDragX_;
    const int deltaY = screenY - lastDragY_;
    camera_.positionX -= static_cast<float>(deltaX) / camera_.zoom;
    camera_.positionY -= static_cast<float>(deltaY) / camera_.zoom;
    lastDragX_ = screenX;
    lastDragY_ = screenY;
}

void CameraController::EndDrag() {
    dragging_ = false;
}

void CameraController::AdjustZoom(int wheelDelta) {
    const float zoomChange = wheelDelta * zoomStep_;
    camera_.zoom = std::clamp(camera_.zoom + zoomChange, minZoom_, maxZoom_);
}

const CameraState& CameraController::GetState() const {
    return camera_;
}

void CameraController::SetState(const CameraState& state) {
    camera_ = state;
}

}  // namespace level_editor

