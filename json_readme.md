# JSON Configuration Reference

This document provides a comprehensive guide to configuring game objects and sprites using JSON files.

## Table of Contents
- [Level Files (level1.json)](#level-files)
- [Sprite Data (spriteData.json)](#sprite-data)
- [Components Reference](#components-reference)
  - [BodyComponent](#bodycomponent)
  - [SpriteComponent](#spritecomponent)
  - [InputComponent](#inputcomponent)
  - [PlayerMovementComponent](#playermovementcomponent)
  - [BoxBehaviorComponent](#boxbehaviorcomponent)
- [Common Examples](#common-examples)

---

## Level Files

Level files define all objects in a scene. The root structure is:

```json
{
    "objects": [
        {
            "components": [ /* array of component objects */ ]
        }
    ]
}
```

Each object is composed of one or more **components** that define its behavior.

---

## Sprite Data

The `spriteData.json` file maps texture files to named sprites with frame data for animation.

### Structure

```json
{
  "textures": {
    "filename.png": {
      "sprites": {
        "sprite_name": {
          "x": 0,
          "y": 0,
          "w": 32,
          "h": 32,
          "frames": [
            { "x": 0, "y": 0, "w": 32, "h": 32 },
            { "x": 32, "y": 0, "w": 32, "h": 32 }
          ]
        }
      }
    }
  }
}
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `textures` | Object | Maps texture filenames to sprite definitions |
| `sprites` | Object | Named sprites within a texture |
| `x, y, w, h` | Number | Default sprite bounds (pixels) |
| `frames` | Array | Array of frame definitions for animation |

### Example: Animated Sprite

```json
"player.png": {
  "sprites": {
    "player_walking": {
      "x": 0,
      "y": 0,
      "w": 32,
      "h": 32,
      "frames": [
        { "x": 0, "y": 0, "w": 32, "h": 32 },
        { "x": 32, "y": 0, "w": 32, "h": 32 },
        { "x": 64, "y": 0, "w": 32, "h": 32 }
      ]
    }
  }
}
```

---

## Components Reference

### BodyComponent

Provides physics simulation using Box2D. Objects with this component will have position, velocity, rotation, and collision.

#### Basic Structure

```json
{
    "type": "BodyComponent",
    "posX": 400.0,
    "posY": 300.0,
    "angle": 0.0,
    "bodyType": "dynamic",
    "fixedRotation": false,
    "bullet": false,
    "linearDamping": 0.5,
    "angularDamping": 0.3,
    "gravityScale": 1.0,
    "fixture": { /* see Fixture section */ }
}
```

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `posX` | Number | 0.0 | Initial X position in pixels |
| `posY` | Number | 0.0 | Initial Y position in pixels |
| `angle` | Number | 0.0 | Initial rotation in radians |
| `bodyType` | String | "dynamic" | Body type: "static", "dynamic", or "kinematic" |
| `fixedRotation` | Boolean | false | If true, prevents rotation |
| `bullet` | Boolean | false | Enables continuous collision detection (for fast objects) |
| `linearDamping` | Number | 0.5 | Reduces linear velocity over time (0-∞, higher = more resistance) |
| `angularDamping` | Number | 0.3 | Reduces angular velocity over time (0-∞, higher = more resistance) |
| `gravityScale` | Number | 1.0 | Multiplier for world gravity (0 = no gravity, 2 = double gravity) |
| `fixture` | Object | Auto | Collision shape definition (see below) |

#### Body Types

- **`static`**: Immovable objects (walls, floors, obstacles)
  - Zero mass, infinite inertia
  - Never moves or rotates
  - Doesn't respond to forces

- **`dynamic`**: Fully simulated objects (players, crates, projectiles)
  - Has mass and velocity
  - Affected by forces and gravity
  - Collides with all body types

- **`kinematic`**: Scriptable objects (moving platforms, doors)
  - Infinite mass
  - Moved by setting velocity, not forces
  - Doesn't respond to collisions but affects others

#### Fixture (Collision Shape)

Defines the physical shape and material properties.

```json
"fixture": {
    "shape": "box",
    "width": 32.0,
    "height": 32.0,
    "density": 1.0,
    "isSensor": false
}
```

##### Fixture Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `shape` | String | "box" | "box" or "circle" |
| `width` | Number | 32.0 | Box width in pixels (box only) |
| `height` | Number | 32.0 | Box height in pixels (box only) |
| `radius` | Number | 16.0 | Circle radius in pixels (circle only) |
| `density` | Number | 1.0 | Mass per area (0 = massless for static) |
| `isSensor` | Boolean | false | If true, detects collisions but doesn't physically block |

**Note**: Friction and restitution are planned for future Box2D v3 API updates.

#### Legacy Format (Still Supported)

For backward compatibility, you can use the old format:

```json
{
    "type": "BodyComponent",
    "posX": 400.0,
    "posY": 300.0,
    "drag": 0.95
}
```

This creates a dynamic body with default 32x32 box fixture and custom damping.

---

### SpriteComponent

Renders a sprite at the object's position. Can work with or without a BodyComponent.

#### Structure

```json
{
    "type": "SpriteComponent",
    "spriteName": "player_walking",
    "animating": true,
    "looping": true,
    "currentFrame": 0,
    "animationSpeed": 10.0,
    "alpha": 255,
    "posX": 100.0,
    "posY": 100.0,
    "angle": 0.0
}
```

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `spriteName` | String | "" | Name of sprite from spriteData.json |
| `animating` | Boolean | false | Whether animation is playing |
| `looping` | Boolean | false | Whether animation loops |
| `currentFrame` | Number | 0 | Starting frame index |
| `animationSpeed` | Number | 10.0 | Frames per second for animation |
| `alpha` | Number | 255 | Transparency (0=invisible, 255=opaque) |
| `posX` | Number | 0.0 | X position (only used if no BodyComponent) |
| `posY` | Number | 0.0 | Y position (only used if no BodyComponent) |
| `angle` | Number | 0.0 | Rotation in radians (only used if no BodyComponent) |

#### Behavior

- **With BodyComponent**: Position is automatically synced from physics body
- **Without BodyComponent**: Uses local `posX`, `posY`, `angle` properties
- Perfect for decorative objects that don't need physics (trees, clouds, etc.)

---

### InputComponent

Connects an input source (keyboard or gamepad) to an object.

#### Structure

```json
{
    "type": "InputComponent",
    "inputSources": [-1]
}
```

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `inputSources` | Array | [] | Array of input device IDs |

#### Input Source IDs

- **-1**: Keyboard
- **0**: First gamepad
- **1**: Second gamepad
- **2**: Third gamepad
- etc.

#### Example: Multi-input

```json
{
    "type": "InputComponent",
    "inputSources": [-1, 0]  // Accept keyboard OR gamepad 0
}
```

---

### PlayerMovementComponent

Handles player movement based on input. Requires InputComponent and BodyComponent.

#### Structure

```json
{
    "type": "PlayerMovementComponent",
    "moveSpeed": 20.0
}
```

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `moveSpeed` | Number | 200.0 | Movement speed in pixels/second |

#### Features

- Applies forces to physics body based on input
- Smooth rotation toward movement direction
- Walk modifier support (slower movement when walk key held)
- Automatic sprite animation switching (standing/walking)

---

### BoxBehaviorComponent

Placeholder component for box-specific behavior. Currently minimal as Box2D handles collision automatically.

#### Structure

```json
{
    "type": "BoxBehaviorComponent",
    "pushForce": 100.0
}
```

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `pushForce` | Number | 100.0 | Reserved for future push mechanics |

#### Notes

- Collision detection and response is automatic via Box2D
- This component can be extended for custom box behaviors (breaking, sounds, etc.)

---

## Common Examples

### Player Character

Circle collision for smooth movement, controlled by keyboard:

```json
{
    "components": [
        {
            "type": "SpriteComponent",
            "spriteName": "player_standing",
            "animating": true,
            "looping": true
        },
        {
            "type": "InputComponent",
            "inputSources": [-1]
        },
        {
            "type": "BodyComponent",
            "posX": 400.0,
            "posY": 300.0,
            "bodyType": "dynamic",
            "fixedRotation": false,
            "linearDamping": 2.0,
            "angularDamping": 3.0,
            "fixture": {
                "shape": "circle",
                "radius": 14.0,
                "density": 1.0
            }
        },
        {
            "type": "PlayerMovementComponent",
            "moveSpeed": 20.0
        }
    ]
}
```

### Pushable Crate

Heavy box that can be pushed around:

```json
{
    "components": [
        {
            "type": "BodyComponent",
            "posX": 200.0,
            "posY": 200.0,
            "bodyType": "dynamic",
            "linearDamping": 1.5,
            "angularDamping": 1.0,
            "fixture": {
                "shape": "box",
                "width": 32.0,
                "height": 32.0,
                "density": 2.0
            }
        },
        {
            "type": "SpriteComponent",
            "spriteName": "crate"
        },
        {
            "type": "BoxBehaviorComponent"
        }
    ]
}
```

### Static Wall

Immovable barrier:

```json
{
    "components": [
        {
            "type": "BodyComponent",
            "posX": 400.0,
            "posY": 0.0,
            "bodyType": "static",
            "fixture": {
                "shape": "box",
                "width": 800.0,
                "height": 20.0,
                "density": 0.0
            }
        },
        {
            "type": "SpriteComponent",
            "spriteName": "wall"
        }
    ]
}
```

### Decorative Tree (No Physics)

Visual-only object:

```json
{
    "components": [
        {
            "type": "SpriteComponent",
            "spriteName": "tree",
            "posX": 100.0,
            "posY": 100.0,
            "angle": 0.0
        }
    ]
}
```

### Trigger Zone (Sensor)

Invisible area that detects when objects enter:

```json
{
    "components": [
        {
            "type": "BodyComponent",
            "posX": 500.0,
            "posY": 300.0,
            "bodyType": "static",
            "fixture": {
                "shape": "box",
                "width": 50.0,
                "height": 50.0,
                "isSensor": true
            }
        }
    ]
}
```

### Moving Platform (Kinematic)

Platform that moves but isn't affected by collisions:

```json
{
    "components": [
        {
            "type": "BodyComponent",
            "posX": 300.0,
            "posY": 400.0,
            "bodyType": "kinematic",
            "fixture": {
                "shape": "box",
                "width": 100.0,
                "height": 20.0,
                "density": 0.0
            }
        },
        {
            "type": "SpriteComponent",
            "spriteName": "platform"
        }
    ]
}
```

---

## Physics Tips

### Damping Values

- **0.0**: No damping (object slides forever)
- **0.5-1.0**: Light damping (smooth sliding)
- **1.5-2.5**: Medium damping (controlled movement)
- **3.0+**: Heavy damping (quick stop)

### Density Values

- **0.0**: Massless (static/kinematic bodies)
- **0.1-0.5**: Very light (balloons, feathers)
- **1.0**: Standard (water density, default)
- **2.0-5.0**: Heavy (crates, rocks)
- **10.0+**: Very heavy (metal blocks)

### Collision Shape Selection

- **Circle**: Best for rolling objects, smooth movement, players
- **Box**: Best for crates, walls, platforms, most objects

### Gravity Scale

- **0.0**: Floating/space (top-down games)
- **1.0**: Normal gravity
- **0.5**: Low gravity (moon)
- **2.0**: High gravity (heavy feel)

---

## Unit Conversions

The engine uses pixels for positions but Box2D uses meters internally.

**Conversion Factor**: 100 pixels = 1 meter

This is handled automatically, but keep in mind:
- Small objects (< 5 pixels) may have physics issues
- Very large objects (> 10000 pixels) may be unstable
- Sweet spot: 10-500 pixel objects

---

## Best Practices

1. **Component Order**: While order doesn't matter functionally, a logical order helps readability:
   ```
   SpriteComponent → BodyComponent → InputComponent → Behavior Components
   ```

2. **Fixture Sizing**: Match fixture size to sprite for visual accuracy
   - Use slightly smaller fixtures for player-friendly collision (14px radius for 32px sprite)
   - Use exact size for precision objects (walls, platforms)

3. **Performance**: 
   - Use static bodies for immovable objects
   - Decorative objects don't need BodyComponent
   - Limit number of dynamic bodies (< 100 recommended)

4. **Naming Conventions**:
   - Sprite names: `object_state` (e.g., "player_walking")
   - Component types: PascalCase with "Component" suffix

5. **Testing**: Start with higher damping values and adjust down for desired feel

---

## Future Features

Planned additions to the JSON configuration system:

- Friction and restitution (Box2D v3 API)
- Polygon collision shapes
- Collision filtering (category bits, mask bits)
- Joint definitions
- Particle effects
- Sound effects
- Custom component parameters

---

## Error Handling

**Missing Required Components**: Some components require others:
- `PlayerMovementComponent` requires `InputComponent` and `BodyComponent`
- `BoxBehaviorComponent` requires `BodyComponent`

**Invalid Values**: The engine will use defaults for invalid/missing values and print warnings to console.

**Missing Sprites**: If a sprite name doesn't exist in spriteData.json, nothing will render but the object still exists.

---

## Version History

- **v1.1** - Added Box2D v3 integration with new BodyComponent format
- **v1.0** - Initial JSON configuration system

