#pragma once

#include "Component.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <utility>

class Object;

struct RailPoint {
    float x;
    float y;
    bool isStop;
    
    RailPoint() : x(0.0f), y(0.0f), isStop(false) {}
    RailPoint(float x, float y, bool isStop = false) : x(x), y(y), isStop(isStop) {}
};

class RailComponent : public Component {
public:
    explicit RailComponent(Object& parent);
    RailComponent(Object& parent, const nlohmann::json& data);
    ~RailComponent() override = default;

    void update(float deltaTime) override;
    void draw() override;
    void use(Object& instigator) override;

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "RailComponent"; }

    // Get the rail path for debug drawing
    const std::vector<RailPoint>& getPath() const { return path; }
    int getCurrentTargetIndex() const { return currentTargetIndex; }
    bool isMoving() const { return isActive; }

private:
    void initializeFromJson(const nlohmann::json& data);
    void startMovement();
    void updateMovement(float deltaTime);
    float distance(float x1, float y1, float x2, float y2) const;
    
    std::vector<RailPoint> path;
    int currentTargetIndex;
    bool isActive;
    float moveSpeed; // pixels per second
    float arrivalThreshold; // distance threshold to consider arrived
};

