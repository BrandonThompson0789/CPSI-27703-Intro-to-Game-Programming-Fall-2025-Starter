# PathfindingBehaviorComponent

The `PathfindingBehaviorComponent` is an AI-oriented movement driver that emits the
same directional inputs as a physical controller. Other behavior components can
request a world-space destination, and this component will steer the parent
object toward that point using grid-based A* pathfinding that respects Box2D
static bodies.

## Usage

1. Attach the component to any object that already has a `BodyComponent`.
2. Call `setDestination(x, y)` (or the overload that accepts `PathPoint`) from
   another behavior when you want the object to start moving.
3. `StandardMovementBehaviorComponent` and `TankMovementBehaviorComponent`
   automatically prioritize the pathfinding input whenever it is active. Player
   input is still used whenever the pathfinder is idle.
4. Invoke `clearDestination()` to abort the current request. Once the target is
   reached, `isDestinationReached()` becomes `true` and the component releases
   control automatically.

## Configuration Fields

All fields are optional and can be provided via component JSON:

| Property | Default | Description |
| --- | --- | --- |
| `gridCellSize` | 48 | Grid resolution (pixels) for A* search. Smaller cells are more precise but slower. |
| `searchPadding` | 96 | Additional padding (pixels) around the start/goal region that the grid will cover. |
| `maxSearchExtent` | 2048 | Maximum width/height (pixels) of the generated grid. Prevents searching across the entire level unintentionally. |
| `agentRadius` | 24 | Extra clearance added to obstacle bounds to mimic the object's footprint. |
| `waypointAcceptanceDistance` | 24 | Distance threshold (pixels) for advancing to the next waypoint. |
| `targetAcceptanceDistance` | 32 | Distance threshold (pixels) that marks the destination as reached. |
| `repathInterval` | 0.4 | Minimum time (seconds) between automatic re-pathing attempts when stuck. |
| `stuckDistanceThreshold` | 160 | If the object strays farther than this from its current waypoint, a new path is requested. |
| `slowdownDistance` | 120 | Distance (pixels) at which the component begins emitting the "walk" modifier so the movement behavior eases into the target. |
| `directionCostBias.cardinal` / `directionCostBias.diagonal` | 1.0 | Multipliers applied to axis-aligned vs diagonal grid steps. Raising the diagonal bias (for example) encourages axis-first routing, useful for tank-style movement. |
| `directionChangePenalty` | 0.0 | Extra cost (in grid units) added whenever the path direction changes between steps. Higher values favor longer straight lines and fewer turns. |

## Debugging Tips

- Call `hasPath()` / `hasPathFailure()` to determine whether the last path
  computation succeeded.
- `getCurrentPath()` returns the current waypoint list which can be visualized in
  a debug view if needed.
- Because the component only blocks against Box2D **static** bodies (and now
  automatically skips static fixtures that are sensors-only), make sure your
  level geometry uses the appropriate fixture types.
- Enable Box2D debug rendering to see the live path: the path polyline is drawn
  in light blue, the currently tracked waypoint is orange, and the final
  destination is highlighted with a pink crosshair.

## Manual Test Plan

1. Spawn an object that includes `BodyComponent`, `StandardMovementBehaviorComponent`
   (or `TankMovementBehaviorComponent`), and the new pathfinding component.
2. From a temporary behavior or a console hook, call `setDestination` with a point
   far from the agent. Verify that the object begins moving and steers around
   static walls.
3. While the pathfinder is active, provide player input — the movement behavior
   should continue to respect the pathfinder until it reaches the destination or
   you call `clearDestination`.
4. Clear the destination mid-path and confirm the agent stops immediately and the
   player's input resumes control.


