#include "RailComponent.h"

#include "ComponentLibrary.h"
#include "../Object.h"
#include "../Engine.h"
#include "BodyComponent.h"
#include <cmath>
#include <iostream>
#include <limits>

RailComponent::RailComponent(Object& parent)
    : Component(parent)
    , currentTargetIndex(-1)
    , isActive(false)
    , moveSpeed(100.0f)
    , arrivalThreshold(2.0f) {
}

RailComponent::RailComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , currentTargetIndex(-1)
    , isActive(false)
    , moveSpeed(100.0f)
    , arrivalThreshold(2.0f) {
    initializeFromJson(data);
}

void RailComponent::initializeFromJson(const nlohmann::json& data) {
    // Load movement speed
    moveSpeed = data.value("moveSpeed", moveSpeed);
    arrivalThreshold = data.value("arrivalThreshold", arrivalThreshold);
    
    // Load path points
    if (data.contains("path") && data["path"].is_array()) {
        path.clear();
        for (const auto& pointData : data["path"]) {
            if (pointData.is_object()) {
                float x = pointData.value("x", 0.0f);
                float y = pointData.value("y", 0.0f);
                bool isStop = pointData.value("isStop", false);
                path.push_back(RailPoint(x, y, isStop));
            } else if (pointData.is_array() && pointData.size() >= 2) {
                // Support [x, y] or [x, y, isStop] format
                float x = pointData[0].get<float>();
                float y = pointData[1].get<float>();
                bool isStop = pointData.size() >= 3 ? pointData[2].get<bool>() : false;
                path.push_back(RailPoint(x, y, isStop));
            }
        }
    }
    
    // If path is empty, we can't do anything
    if (path.empty()) {
        std::cerr << "Warning: RailComponent has no path points." << std::endl;
    } else {
        // Initialize object position to the first point in the path
        auto* body = parent().getComponent<BodyComponent>();
        if (body && !path.empty()) {
            const RailPoint& firstPoint = path[0];
            auto [currentX, currentY, currentAngle] = body->getPosition();
            body->setPosition(firstPoint.x, firstPoint.y, currentAngle);
        }
    }
}

void RailComponent::update(float deltaTime) {
    if (isActive && !path.empty()) {
        updateMovement(deltaTime);
    }
}

void RailComponent::draw() {
    // Drawing is handled by Box2DDebugDraw
}

void RailComponent::use(Object& instigator) {
    // When triggered, start movement if not already moving
    (void)instigator;
    if (!isActive && !path.empty()) {
        startMovement();
    }
}

void RailComponent::startMovement() {
    if (path.empty()) {
        return;
    }
    
    auto* body = parent().getComponent<BodyComponent>();
    if (!body) {
        return;
    }
    
    // If we haven't started, begin at the first point and move to the second
    if (currentTargetIndex < 0) {
        // Start by moving to the next point (index 1, wrapping around)
        currentTargetIndex = 1 % path.size();
    } else {
        // Move to the next point
        currentTargetIndex = (currentTargetIndex + 1) % path.size();
    }
    
    isActive = true;
}

void RailComponent::updateMovement(float deltaTime) {
    if (path.empty() || currentTargetIndex < 0 || currentTargetIndex >= static_cast<int>(path.size())) {
        isActive = false;
        return;
    }
    
    auto* body = parent().getComponent<BodyComponent>();
    if (!body) {
        isActive = false;
        return;
    }
    
    float remainingDistance = moveSpeed * deltaTime;
    
    // Continue moving until we hit a stop or run out of movement distance
    while (isActive && remainingDistance > 0.0f) {
        const RailPoint& target = path[currentTargetIndex];
        auto [currentX, currentY, currentAngle] = body->getPosition();
        
        // Calculate distance and direction to target
        float dx = target.x - currentX;
        float dy = target.y - currentY;
        float dist = distance(currentX, currentY, target.x, target.y);
        
        // Check if we've reached the target
        if (dist <= arrivalThreshold) {
            // Snap to target position
            body->setPosition(target.x, target.y, currentAngle);
            
            // Check if this is a stop point
            if (target.isStop) {
                isActive = false;
                break;
            } else {
                // Move to next point and continue in the same frame
                currentTargetIndex = (currentTargetIndex + 1) % path.size();
                // Continue loop to immediately process next point (no distance consumed)
            }
        } else {
            // Move towards target
            if (remainingDistance >= dist) {
                // We can reach the target this frame
                body->setPosition(target.x, target.y, currentAngle);
                remainingDistance -= dist;
                
                // Check if it's a stop
                if (target.isStop) {
                    isActive = false;
                    break;
                } else {
                    // Move to next point and continue processing in same frame
                    currentTargetIndex = (currentTargetIndex + 1) % path.size();
                    // Continue loop to process next point
                }
            } else {
                // Move partway towards target
                float ratio = remainingDistance / dist;
                float newX = currentX + dx * ratio;
                float newY = currentY + dy * ratio;
                body->setPosition(newX, newY, currentAngle);
                // We've used up all movement for this frame
                break;
            }
        }
    }
}

float RailComponent::distance(float x1, float y1, float x2, float y2) const {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

nlohmann::json RailComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    j["moveSpeed"] = moveSpeed;
    j["arrivalThreshold"] = arrivalThreshold;
    
    nlohmann::json pathArray = nlohmann::json::array();
    for (const auto& point : path) {
        nlohmann::json pointObj;
        pointObj["x"] = point.x;
        pointObj["y"] = point.y;
        pointObj["isStop"] = point.isStop;
        pathArray.push_back(pointObj);
    }
    j["path"] = pathArray;
    
    return j;
}

// Register this component type with the library
static ComponentRegistrar<RailComponent> registrar("RailComponent");

