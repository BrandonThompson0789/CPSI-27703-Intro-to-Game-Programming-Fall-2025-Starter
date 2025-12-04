#pragma once

#include "level_editor/CameraState.h"

namespace level_editor {

struct CameraInputState {
    bool moveLeft{false};
    bool moveRight{false};
    bool moveUp{false};
    bool moveDown{false};
};

class CameraController {
public:
    CameraController();

    void Update(const CameraInputState& inputState, float deltaSeconds);
    void BeginDrag(int screenX, int screenY);
    void DragTo(int screenX, int screenY);
    void EndDrag();
    void AdjustZoom(int wheelDelta);

    const CameraState& GetState() const;
    void SetState(const CameraState& state);

private:
    CameraState camera_;
    bool dragging_{false};
    int lastDragX_{0};
    int lastDragY_{0};
    float panSpeed_{600.0f};
    float zoomStep_{0.1f};
    float minZoom_{0.1f};
    float maxZoom_{5.0f};
};

}  // namespace level_editor

