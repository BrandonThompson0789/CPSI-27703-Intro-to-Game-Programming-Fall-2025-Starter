#include "PathfindingBehaviorComponent.h"
#include "../BodyComponent.h"
#include "../ComponentLibrary.h"
#include "../../Engine.h"
#include "../../Object.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>

namespace {
constexpr float SQRT_TWO = 1.41421356237f;

struct OpenSetEntry {
    int index = -1;
    float fCost = 0.0f;
    bool operator<(const OpenSetEntry& other) const {
        return fCost > other.fCost;
    }
};
} // namespace

struct PathfindingBehaviorComponent::ObstacleAABB {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool overlaps(float otherMinX, float otherMinY, float otherMaxX, float otherMaxY) const {
        return !(maxX < otherMinX || minX > otherMaxX || maxY < otherMinY || minY > otherMaxY);
    }
};

struct PathfindingBehaviorComponent::GridDefinition {
    float originX = 0.0f;
    float originY = 0.0f;
    float cellSize = 48.0f;
    int columns = 0;
    int rows = 0;
    std::vector<uint8_t> walkable;

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < columns && y < rows;
    }

    int toIndex(int x, int y) const {
        return y * columns + x;
    }

    std::pair<int, int> indexToCell(int index) const {
        int y = index / columns;
        int x = index % columns;
        return {x, y};
    }

    bool worldToCell(float worldX, float worldY, int& outX, int& outY) const {
        outX = static_cast<int>(std::floor((worldX - originX) / cellSize));
        outY = static_cast<int>(std::floor((worldY - originY) / cellSize));
        return inBounds(outX, outY);
    }

    PathPoint indexToWorld(int index) const {
        auto [x, y] = indexToCell(index);
        return cellToWorld(x, y);
    }

    PathPoint cellToWorld(int x, int y) const {
        return PathPoint{
            originX + (static_cast<float>(x) + 0.5f) * cellSize,
            originY + (static_cast<float>(y) + 0.5f) * cellSize};
    }
};

namespace {
struct NodeRecord {
    float gCost = std::numeric_limits<float>::infinity();
    float hCost = 0.0f;
    int parent = -1;
    bool closed = false;
    bool opened = false;
    int directionIndex = -1;
};

} // namespace

PathfindingBehaviorComponent::ObstacleAABB PathfindingBehaviorComponent::inflateBodyAABB(BodyComponent& body, float agentRadius) {
    auto [posX, posY, angleDeg] = body.getPosition();
    auto [width, height] = body.getFixtureSize();
    if (width <= 0.0f) {
        width = 32.0f;
    }
    if (height <= 0.0f) {
        height = 32.0f;
    }

    float halfWidth = width * 0.5f + agentRadius;
    float halfHeight = height * 0.5f + agentRadius;
    float angleRad = Engine::degreesToRadians(angleDeg);
    float cosA = std::cos(angleRad);
    float sinA = std::sin(angleRad);

    float rotatedHalfWidth = std::abs(cosA) * halfWidth + std::abs(sinA) * halfHeight;
    float rotatedHalfHeight = std::abs(sinA) * halfWidth + std::abs(cosA) * halfHeight;

    return {
        posX - rotatedHalfWidth,
        posY - rotatedHalfHeight,
        posX + rotatedHalfWidth,
        posY + rotatedHalfHeight};
}

PathfindingBehaviorComponent::PathfindingBehaviorComponent(Object& parent)
    : Component(parent)
    , body(nullptr)
    , currentWaypointIndex(0)
    , inputActive(false)
    , goalReached(false)
    , pathFailed(false)
    , pathDirty(false)
    , moveUpValue(0.0f)
    , moveDownValue(0.0f)
    , moveLeftValue(0.0f)
    , moveRightValue(0.0f)
    , walkValue(0.0f)
    , timeSinceLastRepath(0.0f)
    , lastWaypointDistance(std::numeric_limits<float>::infinity())
    , gridCellSize(48.0f)
    , searchPadding(96.0f)
    , maxSearchExtent(2048.0f)
    , agentRadius(24.0f)
    , waypointAcceptanceDistance(24.0f)
    , targetAcceptanceDistance(32.0f)
    , repathInterval(0.4f)
    , stuckDistanceThreshold(160.0f)
    , slowdownDistance(120.0f)
    , cardinalCostScale(1.0f)
    , diagonalCostScale(1.0f)
    , turnPenalty(0.0f) {
    resolveDependencies();
}

PathfindingBehaviorComponent::PathfindingBehaviorComponent(Object& parent, const nlohmann::json& data)
    : Component(parent)
    , body(nullptr)
    , currentWaypointIndex(0)
    , inputActive(false)
    , goalReached(false)
    , pathFailed(false)
    , pathDirty(false)
    , moveUpValue(0.0f)
    , moveDownValue(0.0f)
    , moveLeftValue(0.0f)
    , moveRightValue(0.0f)
    , walkValue(0.0f)
    , timeSinceLastRepath(0.0f)
    , lastWaypointDistance(std::numeric_limits<float>::infinity())
    , gridCellSize(data.value("gridCellSize", 48.0f))
    , searchPadding(data.value("searchPadding", 96.0f))
    , maxSearchExtent(data.value("maxSearchExtent", 2048.0f))
    , agentRadius(data.value("agentRadius", 24.0f))
    , waypointAcceptanceDistance(data.value("waypointAcceptanceDistance", 24.0f))
    , targetAcceptanceDistance(data.value("targetAcceptanceDistance", 32.0f))
    , repathInterval(data.value("repathInterval", 0.4f))
    , stuckDistanceThreshold(data.value("stuckDistanceThreshold", 160.0f))
    , slowdownDistance(data.value("slowdownDistance", 120.0f))
    , cardinalCostScale(1.0f)
    , diagonalCostScale(1.0f)
    , turnPenalty(0.0f) {
    resolveDependencies();
    if (data.contains("directionCostBias") && data["directionCostBias"].is_object()) {
        const auto& bias = data["directionCostBias"];
        float cardinal = bias.value("cardinal", 1.0f);
        float diagonal = bias.value("diagonal", 1.0f);
        setDirectionCostBias(cardinal, diagonal);
    }
    if (data.contains("directionChangePenalty")) {
        setTurnCostPenalty(data.value("directionChangePenalty", 0.0f));
    }
}

void PathfindingBehaviorComponent::resolveDependencies() {
    body = parent().getComponent<BodyComponent>();
    if (!body) {
        std::cerr << "Warning: PathfindingBehaviorComponent requires BodyComponent\n";
    }
}

void PathfindingBehaviorComponent::applyIdleInput() {
    moveUpValue = 0.0f;
    moveDownValue = 0.0f;
    moveLeftValue = 0.0f;
    moveRightValue = 0.0f;
    walkValue = 0.0f;
    inputActive = false;
}

void PathfindingBehaviorComponent::update(float deltaTime) {
    if (!body) {
        resolveDependencies();
        if (!body) {
            applyIdleInput();
            return;
        }
    }

    updateNavigation(deltaTime);
}

void PathfindingBehaviorComponent::setDestination(float worldX, float worldY) {
    setDestination(PathPoint{worldX, worldY});
}

void PathfindingBehaviorComponent::setDestination(const PathPoint& point) {
    targetPosition = point;
    goalReached = false;
    pathFailed = false;
    pathDirty = true;
}

void PathfindingBehaviorComponent::clearDestination() {
    targetPosition.reset();
    worldPath.clear();
    currentWaypointIndex = 0;
    goalReached = false;
    pathFailed = false;
    pathDirty = false;
    timeSinceLastRepath = 0.0f;
    lastWaypointDistance = std::numeric_limits<float>::infinity();
    applyIdleInput();
}

void PathfindingBehaviorComponent::updateNavigation(float deltaTime) {
    timeSinceLastRepath += deltaTime;

    if (!targetPosition.has_value()) {
        goalReached = false;
        pathFailed = false;
        worldPath.clear();
        currentWaypointIndex = 0;
        applyIdleInput();
        return;
    }

    auto [bodyX, bodyY, _angle] = body->getPosition();
    float goalDx = targetPosition->x - bodyX;
    float goalDy = targetPosition->y - bodyY;
    float distanceToGoal = std::sqrt(goalDx * goalDx + goalDy * goalDy);
    if (distanceToGoal <= targetAcceptanceDistance) {
        goalReached = true;
        clearDestination();
        return;
    }

    bool shouldAttemptRebuild = false;
    if (pathDirty) {
        shouldAttemptRebuild = true;
    } else if (worldPath.empty()) {
        shouldAttemptRebuild = timeSinceLastRepath >= repathInterval;
    } else {
        float deviation = lastWaypointDistance;
        if (deviation > stuckDistanceThreshold && timeSinceLastRepath >= repathInterval) {
            shouldAttemptRebuild = true;
        }
    }

    if (shouldAttemptRebuild) {
        if (rebuildPath()) {
            timeSinceLastRepath = 0.0f;
            pathDirty = false;
            pathFailed = false;
        } else {
            pathDirty = false;
            pathFailed = true;
            timeSinceLastRepath = 0.0f;
            inputActive = false;
            worldPath.clear();
            currentWaypointIndex = 0;
            applyIdleInput();
            return;
        }
    }

    if (worldPath.empty() || currentWaypointIndex >= worldPath.size()) {
        applyIdleInput();
        inputActive = false;
        return;
    }

    advanceWaypointIfNeeded(bodyX, bodyY);

    if (currentWaypointIndex >= worldPath.size()) {
        applyIdleInput();
        inputActive = false;
        return;
    }

    const PathPoint& waypoint = worldPath[currentWaypointIndex];
    float dirX = waypoint.x - bodyX;
    float dirY = waypoint.y - bodyY;
    float dist = std::sqrt(dirX * dirX + dirY * dirY);
    if (dist > 0.001f) {
        dirX /= dist;
        dirY /= dist;
    } else {
        dirX = 0.0f;
        dirY = 0.0f;
    }

    updateInputFromDirection(dirX, dirY, distanceToGoal);
}

bool PathfindingBehaviorComponent::rebuildPath() {
    if (!body || !targetPosition.has_value()) {
        return false;
    }

    auto [startX, startY, _angle] = body->getPosition();
    bool built = buildPathFrom(startX, startY, targetPosition->x, targetPosition->y);
    if (built) {
        currentWaypointIndex = 0;
        lastWaypointDistance = std::numeric_limits<float>::infinity();
    }
    return built;
}

bool PathfindingBehaviorComponent::buildPathFrom(float startX, float startY, float goalX, float goalY) {
    std::vector<ObstacleAABB> obstacles = collectStaticObstacles();
    GridDefinition grid = buildGrid(obstacles, startX, startY, goalX, goalY);
    if (grid.columns <= 0 || grid.rows <= 0) {
        return false;
    }

    int startCellX = 0;
    int startCellY = 0;
    grid.worldToCell(startX, startY, startCellX, startCellY);
    int goalCellX = 0;
    int goalCellY = 0;
    grid.worldToCell(goalX, goalY, goalCellX, goalCellY);

    if (!ensureWalkableCell(grid, startCellX, startCellY, 3)) {
        return false;
    }
    if (!ensureWalkableCell(grid, goalCellX, goalCellY, 6)) {
        return false;
    }

    int startIndex = grid.toIndex(startCellX, startCellY);
    int goalIndex = grid.toIndex(goalCellX, goalCellY);

    std::vector<int> nodePath;
    if (!runAStar(grid, startIndex, goalIndex, nodePath)) {
        return false;
    }

    worldPath = convertGridPathToWorld(grid, nodePath, targetPosition);
    return !worldPath.empty();
}

std::vector<PathfindingBehaviorComponent::ObstacleAABB> PathfindingBehaviorComponent::collectStaticObstacles() const {
    std::vector<ObstacleAABB> obstacles;
    Engine* engine = Object::getEngine();
    if (!engine) {
        return obstacles;
    }

    auto gather = [&](const std::vector<std::unique_ptr<Object>>& container) {
        for (const auto& obj : container) {
            if (!obj) {
                continue;
            }
            if (obj.get() == &parent()) {
                continue;
            }
            BodyComponent* obstacleBody = obj->getComponent<BodyComponent>();
            if (!obstacleBody || !obstacleBody->isStaticBody() || obstacleBody->hasOnlySensorFixtures()) {
                continue;
            }
            obstacles.push_back(inflateBodyAABB(*obstacleBody, agentRadius));
        }
    };

    gather(engine->getObjects());
    gather(engine->getQueuedObjects());

    return obstacles;
}

PathfindingBehaviorComponent::GridDefinition PathfindingBehaviorComponent::buildGrid(
    const std::vector<ObstacleAABB>& obstacles,
    float startX,
    float startY,
    float goalX,
    float goalY) const {

    GridDefinition grid;
    grid.cellSize = std::max(8.0f, gridCellSize);

    float minX = std::min(startX, goalX);
    float maxX = std::max(startX, goalX);
    float minY = std::min(startY, goalY);
    float maxY = std::max(startY, goalY);

    for (const auto& obstacle : obstacles) {
        minX = std::min(minX, obstacle.minX);
        maxX = std::max(maxX, obstacle.maxX);
        minY = std::min(minY, obstacle.minY);
        maxY = std::max(maxY, obstacle.maxY);
    }

    minX -= searchPadding;
    minY -= searchPadding;
    maxX += searchPadding;
    maxY += searchPadding;

    float width = maxX - minX;
    float height = maxY - minY;
    if (width > maxSearchExtent) {
        float centerX = (startX + goalX) * 0.5f;
        minX = centerX - maxSearchExtent * 0.5f;
        maxX = centerX + maxSearchExtent * 0.5f;
        width = maxSearchExtent;
    }
    if (height > maxSearchExtent) {
        float centerY = (startY + goalY) * 0.5f;
        minY = centerY - maxSearchExtent * 0.5f;
        maxY = centerY + maxSearchExtent * 0.5f;
        height = maxSearchExtent;
    }

    grid.originX = minX;
    grid.originY = minY;
    grid.columns = std::max(1, static_cast<int>(std::ceil(width / grid.cellSize)));
    grid.rows = std::max(1, static_cast<int>(std::ceil(height / grid.cellSize)));
    grid.walkable.assign(grid.columns * grid.rows, 1);

    for (const auto& obstacle : obstacles) {
        if (!obstacle.overlaps(minX, minY, maxX, maxY)) {
            continue;
        }
        int startCol = static_cast<int>(std::floor((obstacle.minX - minX) / grid.cellSize));
        int endCol = static_cast<int>(std::floor((obstacle.maxX - minX) / grid.cellSize));
        int startRow = static_cast<int>(std::floor((obstacle.minY - minY) / grid.cellSize));
        int endRow = static_cast<int>(std::floor((obstacle.maxY - minY) / grid.cellSize));

        startCol = std::clamp(startCol, 0, grid.columns - 1);
        endCol = std::clamp(endCol, 0, grid.columns - 1);
        startRow = std::clamp(startRow, 0, grid.rows - 1);
        endRow = std::clamp(endRow, 0, grid.rows - 1);

        for (int y = startRow; y <= endRow; ++y) {
            for (int x = startCol; x <= endCol; ++x) {
                grid.walkable[grid.toIndex(x, y)] = 0;
            }
        }
    }

    return grid;
}

bool PathfindingBehaviorComponent::runAStar(const GridDefinition& grid, int startIndex, int goalIndex, std::vector<int>& outPath) const {
    const int totalNodes = grid.columns * grid.rows;
    std::vector<NodeRecord> nodes(totalNodes);
    std::priority_queue<OpenSetEntry> openSet;

    nodes[startIndex].gCost = 0.0f;
    auto [goalX, goalY] = grid.indexToCell(goalIndex);
    auto [startX, startY] = grid.indexToCell(startIndex);
    nodes[startIndex].hCost = std::hypot(static_cast<float>(goalX - startX), static_cast<float>(goalY - startY));
    nodes[startIndex].opened = true;

    openSet.push({startIndex, nodes[startIndex].hCost});

    const int neighborOffsets[8][2] = {
        {1, 0},  {-1, 0}, {0, 1},  {0, -1},
        {1, 1},  {1, -1}, {-1, 1}, {-1, -1}};

    while (!openSet.empty()) {
        OpenSetEntry current = openSet.top();
        openSet.pop();

        if (nodes[current.index].closed) {
            continue;
        }
        nodes[current.index].closed = true;

        if (current.index == goalIndex) {
            int trace = goalIndex;
            outPath.clear();
            while (trace != -1) {
                outPath.push_back(trace);
                trace = nodes[trace].parent;
            }
            std::reverse(outPath.begin(), outPath.end());
            return true;
        }

        auto [currentX, currentY] = grid.indexToCell(current.index);
        for (int dirIndex = 0; dirIndex < 8; ++dirIndex) {
            int neighborX = currentX + neighborOffsets[dirIndex][0];
            int neighborY = currentY + neighborOffsets[dirIndex][1];
            if (!grid.inBounds(neighborX, neighborY)) {
                continue;
            }

            bool isDiagonal = neighborOffsets[dirIndex][0] != 0 && neighborOffsets[dirIndex][1] != 0;
            if (!grid.walkable[grid.toIndex(neighborX, neighborY)]) {
                continue;
            }

            if (isDiagonal) {
                int adjX = currentX + neighborOffsets[dirIndex][0];
                int adjY = currentY;
                int adjX2 = currentX;
                int adjY2 = currentY + neighborOffsets[dirIndex][1];
                if (!grid.inBounds(adjX, adjY) || !grid.inBounds(adjX2, adjY2)) {
                    continue;
                }
                if (!grid.walkable[grid.toIndex(adjX, adjY)] || !grid.walkable[grid.toIndex(adjX2, adjY2)]) {
                    continue;
                }
            }

            float costScale = isDiagonal ? diagonalCostScale : cardinalCostScale;
            float movementCost = (isDiagonal ? SQRT_TWO : 1.0f) * costScale;
            float turnCost = 0.0f;
            if (turnPenalty > 0.0f && nodes[current.index].directionIndex != -1 && nodes[current.index].directionIndex != dirIndex) {
                turnCost = turnPenalty;
            }
            float tentativeG = nodes[current.index].gCost + movementCost + turnCost;

            int neighborIndex = grid.toIndex(neighborX, neighborY);
            if (!nodes[neighborIndex].opened || tentativeG < nodes[neighborIndex].gCost) {
                nodes[neighborIndex].gCost = tentativeG;
                nodes[neighborIndex].hCost =
                    std::hypot(static_cast<float>(goalX - neighborX), static_cast<float>(goalY - neighborY));
                nodes[neighborIndex].parent = current.index;
                nodes[neighborIndex].opened = true;
                nodes[neighborIndex].directionIndex = dirIndex;
                openSet.push({neighborIndex, tentativeG + nodes[neighborIndex].hCost});
            }
        }
    }

    return false;
}

bool PathfindingBehaviorComponent::ensureWalkableCell(const GridDefinition& grid, int& cellX, int& cellY, int searchRadius) const {
    cellX = std::clamp(cellX, 0, grid.columns - 1);
    cellY = std::clamp(cellY, 0, grid.rows - 1);

    auto isWalkable = [&](int x, int y) {
        return grid.walkable[grid.toIndex(x, y)] != 0;
    };

    if (isWalkable(cellX, cellY)) {
        return true;
    }

    for (int radius = 1; radius <= searchRadius; ++radius) {
        int minX = std::max(0, cellX - radius);
        int maxX = std::min(grid.columns - 1, cellX + radius);
        int minY = std::max(0, cellY - radius);
        int maxY = std::min(grid.rows - 1, cellY + radius);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (isWalkable(x, y)) {
                    cellX = x;
                    cellY = y;
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<PathfindingBehaviorComponent::PathPoint> PathfindingBehaviorComponent::convertGridPathToWorld(
    const GridDefinition& grid,
    const std::vector<int>& path,
    std::optional<PathPoint> goalOverride) const {
    std::vector<PathPoint> result;
    result.reserve(path.size());

    for (int index : path) {
        result.push_back(grid.indexToWorld(index));
    }

    if (!result.empty() && goalOverride.has_value()) {
        result.back() = goalOverride.value();
    }

    // Remove redundant intermediate points that lie on the same line
    if (result.size() >= 3) {
        std::vector<PathPoint> simplified;
        simplified.reserve(result.size());
        simplified.push_back(result.front());
        for (size_t i = 1; i + 1 < result.size(); ++i) {
            PathPoint prev = simplified.back();
            PathPoint curr = result[i];
            PathPoint next = result[i + 1];

            float v1x = curr.x - prev.x;
            float v1y = curr.y - prev.y;
            float v2x = next.x - curr.x;
            float v2y = next.y - curr.y;
            float cross = v1x * v2y - v1y * v2x;
            if (std::abs(cross) > 0.01f) {
                simplified.push_back(curr);
            }
        }
        simplified.push_back(result.back());
        result.swap(simplified);
    }

    return result;
}

void PathfindingBehaviorComponent::advanceWaypointIfNeeded(float bodyX, float bodyY) {
    while (currentWaypointIndex < worldPath.size()) {
        const PathPoint& waypoint = worldPath[currentWaypointIndex];
        float dx = waypoint.x - bodyX;
        float dy = waypoint.y - bodyY;
        float distance = std::sqrt(dx * dx + dy * dy);
        lastWaypointDistance = distance;
        if (distance <= waypointAcceptanceDistance && currentWaypointIndex + 1 < worldPath.size()) {
            ++currentWaypointIndex;
            continue;
        }
        break;
    }
}

void PathfindingBehaviorComponent::updateInputFromDirection(float dirX, float dirY, float distanceToGoal) {
    inputActive = (std::abs(dirX) > 0.0f || std::abs(dirY) > 0.0f);

    moveUpValue = dirY < 0.0f ? -dirY : 0.0f;
    moveDownValue = dirY > 0.0f ? dirY : 0.0f;
    moveLeftValue = dirX < 0.0f ? -dirX : 0.0f;
    moveRightValue = dirX > 0.0f ? dirX : 0.0f;

    moveUpValue = std::clamp(moveUpValue, 0.0f, 1.0f);
    moveDownValue = std::clamp(moveDownValue, 0.0f, 1.0f);
    moveLeftValue = std::clamp(moveLeftValue, 0.0f, 1.0f);
    moveRightValue = std::clamp(moveRightValue, 0.0f, 1.0f);

    if (slowdownDistance > 0.0f && distanceToGoal < slowdownDistance) {
        float t = 1.0f - (distanceToGoal / slowdownDistance);
        walkValue = std::clamp(t, 0.0f, 1.0f);
    } else {
        walkValue = 0.0f;
    }

    if (!inputActive) {
        applyIdleInput();
    }
}

nlohmann::json PathfindingBehaviorComponent::toJson() const {
    nlohmann::json data;
    data["type"] = getTypeName();
    data["gridCellSize"] = gridCellSize;
    data["searchPadding"] = searchPadding;
    data["maxSearchExtent"] = maxSearchExtent;
    data["agentRadius"] = agentRadius;
    data["waypointAcceptanceDistance"] = waypointAcceptanceDistance;
    data["targetAcceptanceDistance"] = targetAcceptanceDistance;
    data["repathInterval"] = repathInterval;
    data["stuckDistanceThreshold"] = stuckDistanceThreshold;
    data["slowdownDistance"] = slowdownDistance;
    data["directionCostBias"] = {
        {"cardinal", cardinalCostScale},
        {"diagonal", diagonalCostScale}
    };
    data["directionChangePenalty"] = turnPenalty;
    return data;
}

std::optional<PathfindingBehaviorComponent::PathPoint> PathfindingBehaviorComponent::getActiveWaypoint() const {
    if (worldPath.empty() || currentWaypointIndex >= worldPath.size()) {
        return std::nullopt;
    }
    return worldPath[currentWaypointIndex];
}

void PathfindingBehaviorComponent::setDirectionCostBias(float cardinalScale, float diagonalScale) {
    const auto clampScale = [](float value) {
        return std::clamp(value, 0.1f, 10.0f);
    };
    cardinalCostScale = clampScale(cardinalScale);
    diagonalCostScale = clampScale(diagonalScale);
}

void PathfindingBehaviorComponent::setTurnCostPenalty(float penalty) {
    turnPenalty = std::clamp(penalty, 0.0f, 100.0f);
}

static ComponentRegistrar<PathfindingBehaviorComponent> registrar("PathfindingBehaviorComponent");

