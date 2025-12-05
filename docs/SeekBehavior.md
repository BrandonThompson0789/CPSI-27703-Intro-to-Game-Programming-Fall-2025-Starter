# SeekBehaviorComponent

The `SeekBehaviorComponent` finds the nearest object whose name matches a
configured list (optionally regex-driven) and hands the position off to the
`PathfindingBehaviorComponent`, which drives movement input. Combine it with
`PathfindingBehaviorComponent` and a movement behavior (standard or tank) to
create autonomous seekers.

## Configuration

All fields are optional with sensible defaults:

| Property | Type | Default | Description |
| --- | --- | --- | --- |
| `targetNames` | string or array | _required_ | Allowed names or regex patterns. |
| `useRegex` | bool | `false` | Treat `targetNames` entries as ECMAScript regex (case-insensitive). |
| `allowSelfTarget` | bool | `false` | Permit the seeker to pick itself (rarely desired). |
| `retargetInterval` | float | `0.5` | Seconds between automatic retarget attempts when no target is locked. |
| `destinationUpdateThreshold` | float | `24` | Minimum distance (pixels) a target must move before issuing a new path destination. |
| `maxSearchDistance` | float | `0` | Optional maximum range (pixels). `0` disables the range check. |

Example component JSON:

```json
{
  "type": "SeekBehaviorComponent",
  "targetNames": ["Player", "Companion.*"],
  "useRegex": true,
  "maxSearchDistance": 1200
}
```

## Behavior

1. On each update the component ensures it has a nearby target that matches the
   configured names (via exact or regex matching, following the conventions
   established by `SensorComponent`).
2. Once a target is chosen, the seeker requests a destination from
   `PathfindingBehaviorComponent`. Destination updates are throttled so the
   pathfinder only recomputes when the target has moved a meaningful distance.
3. If no match is available or the target becomes invalid, the component clears
   the active destination so other drivers (player input, scripts) regain
   control immediately.

## Manual Test Plan

1. Place an NPC with `BodyComponent`, `PathfindingBehaviorComponent`,
   `StandardMovementBehaviorComponent`, and `SeekBehaviorComponent`.
2. Configure `targetNames` to include the player character's name.
3. Run the level and verify the NPC begins moving toward the player, rerouting
   when obstacles intervene.
4. Rename the player or toggle `useRegex` to confirm matching behaves the same
   as `SensorComponent`'s name filters.
5. Destroy or move the target far away; the seeker should smoothly retarget or
   stop as soon as no valid target remains.


