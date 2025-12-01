
At a glance:

Any object with a bodyComponent will have Box2D bodies attached and that component also controls the physics of said body.

Velocities are demo-able via the GrabBehaviorComponent and ThrowBehaviorComponent (Objects can be thrown).

Raycasting is used to determine if a grabbable object is in view, range, and is grabbable.

AABB is used by one of the sensor types 'Box Zone' to determine if an object is within its space.

Another sensor type determines if an object is colliding with it (with other varying parameters).

Objects are loaded from file, more can be added at runtime via the 'ObjectSpawnerComponent' once triggered.

Objects are deletable at runtime by marking them for 'Death', done by loss of health or trigger.

Static bodies are used as walls, Dynamic bodies are most other objects, Kinematic bodies are used for 'on-rail' objects.

![hippo](docs/project04.gif)

At a longer glance:

# Project04 - Physics

## Overview
Project04 focuses entirely on Box2D physics implementation and its integration throughout the game engine. This project demonstrates the use of Box2D for rigid body physics simulation, including body creation and destruction, various body types, physics materials, force application, raycasting, and AABB querying.

## Physics World Initialization

The physics world is initialized in `Engine::init()` with a zero-gravity configuration suitable for top-down gameplay. The physics world is stepped each frame in `Engine::update()` with a step count that provides a good balance between accuracy and performance.

## Body Types

The engine supports all three Box2D body types:

### Dynamic Bodies
- Respond to forces and collisions
- Can be moved by applying forces or setting velocity
- Used for player characters, projectiles, and interactive objects
- Created with `bodyType: "dynamic"` in JSON

### Static Bodies
- Immovable objects that don't respond to forces
- Used for walls, floors, and level geometry
- Created with `bodyType: "static"` in JSON

### Kinematic Bodies
- Can be moved programmatically but don't respond to forces
- Used for moving platforms or scripted objects
- Created with `bodyType: "kinematic"` in JSON

Body types are parsed from JSON strings during body creation.

## Body Creation

### Creation from JSON Files

Bodies are created when loading level files through `Engine::loadFile()`. The `BodyComponent` constructor accepts JSON data and creates bodies with the specified properties.

The JSON format supports:
- Position (`posX`, `posY`, `angle`)
- Body type (`bodyType`: "dynamic", "static", or "kinematic")
- Body properties: `fixedRotation`, `bullet`, `linearDamping`, `angularDamping`, `gravityScale`
- Fixture configuration: shape type (box/circle), dimensions, density, friction, restitution
- Material assignment via material names

Example JSON body configuration:
```json
{
  "type": "BodyComponent",
  "posX": 100.0,
  "posY": 200.0,
  "angle": 45.0,
  "bodyType": "dynamic",
  "linearDamping": 0.5,
  "fixture": {
    "shape": "box",
    "width": 32.0,
    "height": 32.0,
    "density": 1.0,
    "friction": 0.3,
    "material": "wood"
  }
}
```

### Interactive Runtime Creation

Bodies can be created interactively at runtime through the `ObjectSpawnerComponent`. When an object with this component is used (via `Object::use()`), it spawns new objects with physics bodies:

1. `ObjectSpawnerComponent::spawnObject()` selects which object to spawn
2. `ObjectSpawnerComponent::createAndQueueObject()` creates a new `Object` with components
3. The new object's `BodyComponent` is created from JSON data
4. The object is queued via `Engine::queueObject()` and added to the world at the start of the next frame

This allows for dynamic object spawning during gameplay, such as spawning projectiles, power-ups, or environmental objects.

Bodies are also created interactively when:
- Network clients receive object creation messages (via `ClientManager::CreateObjectFromJson()`)
- Explosion effects create debris objects (via `ExplodeOnDeathComponent`)

## Body Destruction

Bodies are automatically destroyed when their parent `Object` is destroyed. The `BodyComponent` destructor handles cleanup, ensuring that physics bodies are properly removed from the physics world.

Objects can be marked for death via `Object::markForDeath()`, and the `Engine::update()` loop removes destroyed objects at the end of each frame, triggering their destructors and thus destroying their physics bodies.

## Fixture Shapes

The engine supports two fixture shape types:

### Box Shapes (Polygons)
- Rectangular fixtures
- Defined by width and height in pixels (converted to meters internally)
- Used for most game objects

### Circle Shapes
- Circular fixtures
- Defined by radius in pixels (converted to meters internally)
- Used for projectiles and round objects

Shapes are created in `BodyComponent::createFixtureFromJson()` based on the `shape` field in JSON data.

## Physics Materials

The engine uses a physics material system (`PhysicsMaterialLibrary`) that allows materials to be assigned to shapes. Materials define:
- Friction coefficients
- Restitution (bounciness)
- Custom material IDs for collision sound selection

Materials are assigned via the `material` field in fixture JSON data, and the material properties are applied to shapes during creation.

## Force Application

Forces are applied to bodies through the `BodyComponent::modVelocity()` method, which modifies the body's velocity. This approach provides immediate response and is well-suited for top-down gameplay.

### Velocity Modification
`BodyComponent::modVelocity()` adds to the current velocity, effectively applying a force impulse:

```cpp
body->modVelocity(velocityX, velocityY, angularVelocity);
```

### Movement Behavior Components

Movement is implemented in behavior components that apply forces via velocity modification:

- **StandardMovementBehaviorComponent**: Applies velocity based on input direction, with rotation toward movement direction
- **TankMovementBehaviorComponent**: Tank-style movement with quantized cardinal directions and rotation before movement
- **ThrowBehaviorComponent**: Applies velocity to thrown objects based on charge time and throw direction

Example from `StandardMovementBehaviorComponent`:
```cpp
float velocityX = normalizedHorizontal * currentSpeed * 0.1f;
float velocityY = normalizedVertical * currentSpeed * 0.1f;
body->modVelocity(velocityX, velocityY, 0.0f);
```

The `modVelocity()` method effectively applies forces by modifying the body's velocity, allowing for responsive movement control while maintaining physics simulation accuracy.

## Raycasting

Raycasting is used in the `GrabBehaviorComponent` to find grabbable objects in front of the player. When the player presses the interact button, a ray is cast from the player's position in the direction they're facing.

The raycast implementation:
- Casts a ray from slightly in front of the player position
- Extends in the player's facing direction for the configured grab distance
- Returns the closest object hit by the ray
- Validates that the hit object is grabbable (dynamic body, within density limits)

This allows players to grab objects at a distance without requiring direct collision, making the grab mechanic more intuitive and responsive.

## AABB Querying

AABB (Axis-Aligned Bounding Box) querying is used in box zone sensors to find all objects within a specified rectangular area. Box zone sensors are configured in `SensorComponent` and can require objects to be either fully inside or partially overlapping the zone.

The AABB query implementation:
- Defines a rectangular zone with min/max X and Y coordinates
- Queries the physics world for all shapes overlapping the zone
- Validates objects against sensor conditions (eligibility, interact requirements, etc.)
- Supports both "fully inside" and "partially overlapping" modes

Box zone sensors are useful for:
- Area-of-effect triggers
- Zone-based objectives
- Spatial condition checking
- Level boundaries and checkpoints

Box zones are configured in JSON with `requireBoxZone: true` and a `boxZone` object defining the bounds:
```json
{
  "requireBoxZone": true,
  "boxZone": {
    "minX": -100.0,
    "minY": -50.0,
    "maxX": 100.0,
    "maxY": 50.0,
    "requireFull": false
  }
}
```

## Collision Detection

Collision detection is handled by Box2D and processed through `CollisionManager`. The manager:
- Gathers contact events from the physics world
- Processes hit events to create `CollisionImpact` records
- Applies damage based on collision properties
- Triggers sound effects based on material combinations

Contact events are enabled on bodies and shapes during creation to ensure collision information is available for gameplay systems.

## Coordinate System

The engine uses a pixel-to-meter conversion system:
- **PIXELS_TO_METERS**: 0.01 (1 pixel = 0.01 meters)
- **METERS_TO_PIXELS**: 100.0 (1 meter = 100 pixels)

All physics operations convert between pixel space (used by game logic) and meter space (used by the physics engine) automatically in `BodyComponent` methods.

## Debug Visualization

Physics debug drawing is available via `Box2DDebugDraw`, which can be toggled with the F1 key. The debug draw renders:
- Body outlines
- Fixture shapes
- Contact points
- Joints (if any)
- Body labels with object names
- Box zone boundaries (for sensors with box zones)

## Implementation Files

Key files for physics implementation:

- `src/Engine.cpp` - World initialization and stepping
- `src/components/BodyComponent.h/cpp` - Body creation, destruction, and force application
- `src/CollisionManager.cpp` - Collision event processing
- `src/components/ObjectSpawnerComponent.cpp` - Runtime object/body creation
- `src/components/behaviors/GrabBehaviorComponent.cpp` - Raycasting for object grabbing
- `src/components/SensorComponent.cpp` - AABB querying for box zone sensors
- `src/components/behaviors/*.cpp` - Movement via force application (modVelocity)
- `src/PhysicsMaterial.cpp` - Material system
- `src/Box2DDebugDraw.cpp` - Debug visualization

## Summary

The engine provides comprehensive Box2D physics support with:
- ✅ Multiple body types (dynamic, static, kinematic)
- ✅ Runtime body creation and destruction
- ✅ File-based body loading from JSON
- ✅ Interactive object spawning
- ✅ Force application via velocity modification
- ✅ Raycasting for object detection (grab component)
- ✅ AABB querying for spatial queries (box zone sensors)
- ✅ Collision detection and processing
- ✅ Physics materials
- ✅ Multiple fixture shapes

The physics system is fully integrated into gameplay mechanics, providing robust collision detection, spatial queries, and force-based movement control.
