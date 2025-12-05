#pragma once

#include "../Component.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <utility>
#include <vector>

class BodyComponent;

/**
 * Provides grid-based A* pathfinding that emits virtual movement input.
 * Other components can request destinations and this component will steer
 * its parent object by mimicking controller inputs.
 */
class PathfindingBehaviorComponent : public Component {
public:
    struct PathPoint {
        float x = 0.0f;
        float y = 0.0f;
    };

    PathfindingBehaviorComponent(Object& parent);
    PathfindingBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~PathfindingBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "PathfindingBehaviorComponent"; }

    void setDestination(float worldX, float worldY);
    void setDestination(const PathPoint& point);
    void clearDestination();

    bool hasDestination() const { return targetPosition.has_value(); }
    bool isActive() const { return inputActive; }
    bool hasPath() const { return !worldPath.empty(); }
    bool isDestinationReached() const { return goalReached; }
    bool hasPathFailure() const { return pathFailed; }
    std::optional<PathPoint> getDestinationPoint() const { return targetPosition; }
    std::optional<PathPoint> getActiveWaypoint() const;
    void setDirectionCostBias(float cardinalScale, float diagonalScale);
    std::pair<float, float> getDirectionCostBias() const { return {cardinalCostScale, diagonalCostScale}; }
    void setTurnCostPenalty(float penalty);
    float getTurnCostPenalty() const { return turnPenalty; }

    float getMoveUp() const { return moveUpValue; }
    float getMoveDown() const { return moveDownValue; }
    float getMoveLeft() const { return moveLeftValue; }
    float getMoveRight() const { return moveRightValue; }
    float getActionWalk() const { return walkValue; }

    const std::vector<PathPoint>& getCurrentPath() const { return worldPath; }

private:
    struct ObstacleAABB;
    struct GridDefinition;

    void resolveDependencies();
    void applyIdleInput();
    void updateNavigation(float deltaTime);
    bool rebuildPath();
    bool buildPathFrom(float startX, float startY, float goalX, float goalY);
    std::vector<ObstacleAABB> collectStaticObstacles() const;
    GridDefinition buildGrid(const std::vector<ObstacleAABB>& obstacles,
                             float startX,
                             float startY,
                             float goalX,
                             float goalY) const;
    bool runAStar(const GridDefinition& grid, int startIndex, int goalIndex, std::vector<int>& outPath) const;
    bool ensureWalkableCell(const GridDefinition& grid, int& cellX, int& cellY, int searchRadius) const;
    std::vector<PathPoint> convertGridPathToWorld(const GridDefinition& grid,
                                                  const std::vector<int>& path,
                                                  std::optional<PathPoint> goalOverride) const;
    void advanceWaypointIfNeeded(float bodyX, float bodyY);
    void updateInputFromDirection(float dirX, float dirY, float distanceToGoal);
    static ObstacleAABB inflateBodyAABB(BodyComponent& body, float agentRadius);

    BodyComponent* body;
    std::optional<PathPoint> targetPosition;
    std::vector<PathPoint> worldPath;
    size_t currentWaypointIndex;

    bool inputActive;
    bool goalReached;
    bool pathFailed;
    bool pathDirty;

    float moveUpValue;
    float moveDownValue;
    float moveLeftValue;
    float moveRightValue;
    float walkValue;

    float timeSinceLastRepath;
    float lastWaypointDistance;

    float gridCellSize;
    float searchPadding;
    float maxSearchExtent;
    float agentRadius;
    float waypointAcceptanceDistance;
    float targetAcceptanceDistance;
    float repathInterval;
    float stuckDistanceThreshold;
    float slowdownDistance;
    float cardinalCostScale;
    float diagonalCostScale;
    float turnPenalty;
};

