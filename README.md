# Project02 - Components

At a glance:
This project has removed all child classes of the Object class and has switched them to components.
Input configurations, sprite data, and level data are all being read from a JSON file.
Supports multiple inputs (1 keyboard and up to 4 game controllers, each configurable)
Body component determines object size, position, and velocity (Will be reworked when Box2D gets introduced)
Sprites support animation
"BoxBehavior" component introduces temporary physics collisions applications (physics will be replaced by Box2D)
"PlayerMovement" component introduces an object that reacts to InputComponent. It rotates into the direction of its velocity (wobble is silly, might keep it), and changes sprites from player_still to player_walking when moving.

GIF: 2 players interacting with boxes
![hippo](https://i.imgur.com/yUC86q4.gif)

At a really long glance:

## Table of Contents
- [Component System Architecture](#component-system-architecture)
- [Core Components](#core-components)
- [Management Systems](#management-systems)
- [Data Files](#data-files)
- [Usage Examples](#usage-examples)

---

## Component System Architecture

### Component Base Class (`Component.h`)

The abstract `Component` class serves as the foundation for all components in the system. Every component:

- **Holds a reference to its parent Object** - Components can access their parent object and query for other components
- **Implements update/draw methods** - Virtual methods for game loop integration
- **Supports serialization** - Components can serialize to/from JSON for level loading and saving
- **Provides type information** - Each component declares its type name for the registry system

**Key Methods:**
- `update(float deltaTime)` - Called every frame to update component logic
- `draw()` - Called every frame to render component visuals
- `toJson()` - Serialize component state to JSON
- `getTypeName()` - Returns the component's type identifier

### ComponentLibrary (`ComponentLibrary.h/.cpp`)

A singleton registry that enables dynamic component creation from JSON data using the factory pattern.

**Features:**
- **Automatic Registration** - Components self-register using the `ComponentRegistrar<T>` template helper
- **Factory Pattern** - Creates components from type names and JSON data
- **Type Safety** - Maps string type names to C++ type indices

**How Components Register:**
Each component implementation includes a static registrar:
```cpp
static ComponentRegistrar<MyComponent> registrar("MyComponent");
```

This automatically registers the component type when the program starts, enabling serialization and deserialization.

### Object Class (`Object.h/.cpp`)

The `Object` class is a container for components, implementing the Entity-Component pattern.

**Features:**
- **Component Management** - Add, retrieve, and check for components by type
- **Template-based API** - Type-safe component operations using C++ templates
- **Serialization Support** - Save/load entire objects with all their components
- **Component Map** - Fast component lookup by type using `std::type_index`

**Key Methods:**
```cpp
T* addComponent<T>(Args&&... args)  // Add a component with constructor arguments
T* getComponent<T>()                 // Get a component by type
bool hasComponent<T>()               // Check if object has a component
```

---

## Core Components

### 1. BodyComponent (`BodyComponent.h/.cpp`)

Handles physics-based positioning, velocity, and rotation for game objects.

**Functionality:**
- **Position & Rotation** - Tracks 2D position (x, y) and rotation angle
- **Velocity** - Linear velocity (x, y) and angular velocity
- **Drag Simulation** - Applies speed-dependent drag for realistic deceleration
- **Physics Integration** - Updates position based on velocity each frame

**Properties:**
- `posX, posY, angle` - Position and rotation state
- `velX, velY, velAngle` - Velocity state
- `drag` - Drag coefficient (default 0.95 = 5% reduction per frame)

**Key Features:**
- Speed-dependent drag increases with movement speed for more realistic physics
- Prevents floating-point drift with minimum velocity thresholds
- Provides getters/setters for both absolute and relative velocity changes

**Usage:** Required by most interactive game objects. Other components (like `SpriteComponent`) query `BodyComponent` for position data.

### 2. SpriteComponent (`SpriteComponent.h/.cpp`)

Renders sprites with support for sprite sheet animations.

**Functionality:**
- **Sprite Rendering** - Displays sprites from the `SpriteManager`
- **Frame Animation** - Supports multi-frame sprite animations
- **Animation Control** - Play, pause, loop, and control animation speed
- **Rendering Effects** - Horizontal/vertical flipping and alpha transparency
- **Position Integration** - Automatically queries parent's `BodyComponent` for render position

**Properties:**
- `spriteName` - Reference to sprite in `SpriteManager`
- `currentFrame` - Current animation frame index
- `animating` - Whether animation is currently playing
- `looping` - Whether animation loops when it reaches the end
- `animationSpeed` - Frames per second for animation playback
- `flipFlags` - SDL flip flags (horizontal/vertical)
- `alpha` - Transparency value (0-255)

**Key Methods:**
- `setCurrentSprite()` - Switch to a different sprite
- `playAnimation(bool loop)` - Start animation playback
- `setAnimationSpeed(float fps)` - Control animation speed
- `setFlipHorizontal/Vertical(bool)` - Mirror sprite rendering

### 3. InputComponent (`InputComponent.h/.cpp`)

Provides input handling with support for multiple input sources (keyboard, mouse, game controllers).

**Functionality:**
- **Multi-Source Input** - Supports keyboard and up to 4 game controllers simultaneously
- **Unified Action System** - Maps physical inputs to logical game actions
- **Source Priority** - When multiple sources are active, returns the highest input value
- **Hot-Plug Support** - Handles controller connection/disconnection at runtime

**Properties:**
- `inputSources` - Vector of input source IDs (-1 for keyboard, 0-3 for controllers)
- `inputManager` - Reference to singleton `InputManager`

**Game Actions Supported:**
- Movement: `MOVE_UP`, `MOVE_DOWN`, `MOVE_LEFT`, `MOVE_RIGHT`
- Actions: `ACTION_WALK`, `ACTION_INTERACT`, `ACTION_THROW`

**Key Methods:**
- `getInput(GameAction)` - Returns input value (0.0 to 1.0) from any active source
- `isPressed(GameAction)` - Boolean check for button press (threshold > 0.5)
- `getMoveUp/Down/Left/Right()` - Convenience methods for movement
- `isActive()` - Check if any input source is connected

**Multi-Source Behavior:** When multiple input sources are configured, the component automatically returns the highest value from any source, allowing seamless switching between keyboard and controller.

### 4. PlayerMovementComponent (`PlayerMovementComponent.h/.cpp`)

Implements player character movement with input-driven physics.

**Functionality:**
- **Input-Driven Movement** - Reads from `InputComponent` to apply movement forces
- **Walk Modifier** - Reduces speed when walk button is held (50% speed reduction)
- **Rotation Toward Movement** - Smoothly rotates character to face movement direction
- **Animation Integration** - Switches between standing and walking sprites based on velocity
- **Component Dependencies** - Requires `InputComponent`, `BodyComponent`, and optionally `SpriteComponent`

**Properties:**
- `moveSpeed` - Base movement speed in pixels per second (default 200.0)
- `input` - Cached pointer to parent's `InputComponent`
- `body` - Cached pointer to parent's `BodyComponent`
- `sprite` - Cached pointer to parent's `SpriteComponent`

**Movement Logic:**
1. Reads directional input from `InputComponent`
2. Applies walk modifier if walk button is pressed
3. Adds velocity to `BodyComponent` based on input and speed
4. Calculates rotation to face movement direction
5. Updates sprite animation based on movement speed

**Key Features:**
- Smooth rotation using angular velocity interpolation
- Prevents jittering when standing still
- Integrates with both keyboard and controller input seamlessly

### 5. BoxBehaviorComponent (`BoxBehaviorComponent.h/.cpp`)

Implements pushable box physics with simple collision detection.

**Functionality:**
- **Collision Detection** - Simple AABB (axis-aligned bounding box) collision
- **Push Physics** - Applies force when another moving object collides with the box
- **Speed-Based Response** - Push force scales with the colliding object's speed
- **Multi-Object Interaction** - Checks collision against all objects in the scene

**Properties:**
- `body` - Cached pointer to parent's `BodyComponent`
- `pushForce` - Base force applied when pushed (default 100.0 pixels/second)

**Collision Logic:**
1. Retrieves all objects from the Engine
2. For each object with a `BodyComponent`:
   - Performs AABB collision check
   - If colliding and other object is moving fast enough (> 10.0 speed)
   - Applies push force proportional to other object's speed
   - Direction is away from the colliding object

**Usage:** Typically attached to crate/box objects to make them pushable by the player or other entities.

---

## Management Systems

### SpriteManager (`SpriteManager.h/.cpp`)

Singleton system that manages sprite data and texture loading.

**Responsibilities:**
- **Texture Management** - Loads and caches SDL textures
- **Sprite Data** - Manages sprite frame definitions from JSON
- **Rendering** - Provides unified interface for sprite rendering with rotation, flipping, and alpha

**Data Structure:**
- Stores `SpriteData` objects containing frame information
- Maps sprite names to texture references and frame rectangles
- Loads from `spriteData.json` configuration file

**Key Methods:**
- `init(SDL_Renderer*, spriteDataPath)` - Initialize with renderer and load sprite definitions
- `loadSpriteData(filepath)` - Load sprite definitions from JSON
- `getSpriteData(spriteName)` - Get sprite frame data
- `renderSprite(name, frame, x, y, angle, flip, alpha)` - Render a sprite frame

### InputManager (`InputManager.h/.cpp`)

Singleton system that handles all input processing.

**Responsibilities:**
- **Input Processing** - Polls SDL for keyboard and controller input each frame
- **Action Mapping** - Converts raw input to game actions via `InputConfig`
- **Controller Management** - Handles controller hot-plugging and multiple controllers
- **Configuration** - Loads and manages input mappings from JSON files

**Features:**
- Supports keyboard and up to 4 simultaneous game controllers
- Per-source configuration - different controllers can have different mappings
- Deadzone handling for analog sticks and triggers
- D-pad can be treated as buttons or analog axes

**Key Methods:**
- `init(configPath)` - Initialize with configuration file
- `update()` - Process input each frame (call before component updates)
- `getInputValue(source, action)` - Get current input value for an action
- `isInputSourceActive(source)` - Check if input device is connected
- `handleControllerAdded/Removed()` - Hot-plug support

**Input Sources:**
- `-1` - Keyboard/Mouse
- `0-3` - Controller indices

### InputConfig (`InputConfig.h/.cpp`)

Configuration class for input mappings.

**Responsibilities:**
- **Keyboard Mapping** - Maps keyboard scancodes to game actions
- **Controller Mapping** - Maps controller buttons and axes to game actions
- **Settings** - Deadzone values and other input settings
- **Serialization** - Load/save configurations from/to JSON files

**Mapping Types:**
- **Button Mapping** - Maps one or more buttons to an action (returns 0.0 or 1.0)
- **Axis Mapping** - Maps analog stick or trigger to an action (returns 0.0 to 1.0)
  - Supports positive/negative axis directions
  - Supports full-range (triggers) or split-range (sticks)

**Configuration Options:**
- `deadzone` - Threshold for analog input to register (default ~0.15)
- `dpadAsAxis` - Whether D-pad should behave like analog stick

---

## Data Files

### spriteData.json

Defines sprite sheets and frame positions for all game sprites.

**Structure:**
```json
{
  "textures": {
    "texture_filename.png": {
      "sprites": {
        "sprite_name": {
          "frames": [
            { "x": 0, "y": 0, "w": 32, "h": 32 }
          ]
        }
      }
    }
  }
}
```

**Current Sprites:**
- `player_standing` - Single-frame standing animation
- `player_walking` - 4-frame walking animation
- `crate` - Single-frame crate sprite

**Usage:** Referenced by `SpriteComponent` via sprite name. The `SpriteManager` loads this file at startup.

### level1.json

Defines the game level with all objects and their components.

**Structure:**
```json
{
  "objects": [
    {
      "components": [
        {
          "type": "ComponentType",
          "property1": value1,
          "property2": value2
        }
      ]
    }
  ]
}
```

**Current Level Content:**
- 2 Player objects (keyboard and controller)
- 4 Pushable crates positioned around the level

**Component Serialization:** Each component serializes its full state, allowing levels to specify initial positions, velocities, animation states, etc.

### input_config.json

Defines input mappings for keyboard and controllers.

**Structure:**
```json
{
  "keyboard": {
    "ACTION_NAME": ["KEY1", "KEY2"]
  },
  "controller": {
    "ACTION_NAME": {
      "type": "button" or "axis",
      "buttons": ["BUTTON1"] or
      "axis": "AXIS_NAME",
      "positive": true/false,
      "fullRange": true/false
    }
  },
  "settings": {
    "deadzone": 0.15,
    "dpadAsAxis": true
  }
}
```

**Key Features:**
- Multiple keys/buttons can map to one action
- Axes can map to positive or negative directions
- Separate files allow different control schemes (WASD, arrows, ESDF, etc.)

---

## Usage Examples

### Creating a Game Object with Components

```cpp
// Create a player object
Object* player = new Object();

// Add components in the required order
auto* body = player->addComponent<BodyComponent>();
body->setPosition(400.0f, 300.0f, 0.0f);

auto* sprite = player->addComponent<SpriteComponent>("player_standing", true, true);
auto* input = player->addComponent<InputComponent>(INPUT_SOURCE_KEYBOARD);
auto* movement = player->addComponent<PlayerMovementComponent>(200.0f);

// Object is now fully functional!
```

### Creating a Pushable Box

```cpp
Object* box = new Object();

auto* body = box->addComponent<BodyComponent>();
body->setPosition(150.0f, 200.0f, 0.0f);

auto* sprite = box->addComponent<SpriteComponent>("crate");
auto* behavior = box->addComponent<BoxBehaviorComponent>();

// Box will now respond to collisions with other objects
```

### Loading a Level from JSON

```cpp
// Engine class handles this automatically
engine.loadLevel("assets/level1.json");

// The ComponentLibrary creates all components from their JSON definitions
```

### Querying Components

```cpp
// Get a component from an object
if (auto* body = object->getComponent<BodyComponent>()) {
    auto [x, y, angle] = body->getPosition();
    std::cout << "Object at: " << x << ", " << y << std::endl;
}

// Check if object has a component
if (object->hasComponent<InputComponent>()) {
    // This is a player-controlled object
}
```

---

## Component Dependencies

Some components depend on other components being present on the same object:

| Component | Required Components | Optional Components |
|-----------|-------------------|---------------------|
| `SpriteComponent` | None | `BodyComponent` (for positioning) |
| `BodyComponent` | None | None |
| `InputComponent` | None | None |
| `PlayerMovementComponent` | `InputComponent`, `BodyComponent` | `SpriteComponent` (for animation) |
| `BoxBehaviorComponent` | `BodyComponent` | None |

**Note:** Components that require other components will print warnings if dependencies are missing but will gracefully handle null pointers.

---

## File Organization

```
src/
├── components/
│   ├── Component.h                    # Base component interface
│   ├── ComponentLibrary.h/.cpp        # Component registry and factory
│   ├── BodyComponent.h/.cpp           # Physics component
│   ├── SpriteComponent.h/.cpp         # Rendering component
│   ├── InputComponent.h/.cpp          # Input handling component
│   ├── PlayerMovementComponent.h/.cpp # Player movement logic
│   └── BoxBehaviorComponent.h/.cpp    # Pushable box logic
├── SpriteManager.h/.cpp               # Sprite/texture management
├── InputManager.h/.cpp                # Input system
├── InputConfig.h/.cpp                 # Input configuration
├── Object.h/.cpp                      # Component container
└── Engine.h/.cpp                      # Main game engine

assets/
├── spriteData.json                    # Sprite definitions
├── level1.json                        # Level data
├── input_config.json                  # Default input configuration
├── input_config_arrows.json           # Alternative input configs
└── textures/
    ├── player.png                     # Player sprite sheet
    └── crate.png                      # Crate sprite
```

---

## Extending the System

### Adding a New Component

1. **Create header and implementation files** in `src/components/`
2. **Inherit from Component** and implement required virtual methods
3. **Add serialization** via `toJson()` and JSON constructor
4. **Register the component** using `ComponentRegistrar<YourComponent>`
5. **Update documentation** with component details

Example skeleton:
```cpp
// MyComponent.h
class MyComponent : public Component {
public:
    MyComponent(Object& parent, const nlohmann::json& data);
    void update(float deltaTime) override;
    void draw() override;
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "MyComponent"; }
};

// MyComponent.cpp
static ComponentRegistrar<MyComponent> registrar("MyComponent");
```

### Adding New Game Actions

1. **Add enum value** to `GameAction` in `InputManager.h`
2. **Add mapping helpers** in `InputManager.cpp`
3. **Add convenience method** in `InputComponent.h`
4. **Update input config files** with new action mappings

---

## Technical Notes

- **Component Lifetime** - Components are owned by their parent Object via `unique_ptr`
- **Component Access** - Fast O(1) lookup using `std::type_index` hash map
- **Serialization** - Uses nlohmann/json library for JSON parsing
- **Memory Safety** - Components store raw pointers to sibling components (safe as long as parent Object exists)
- **Registration** - Static registrars ensure components register before `main()` runs
- **Hot-Plugging** - Controllers can be connected/disconnected during gameplay

---

This component system provides a flexible, extensible foundation for game object design with full serialization support and clean separation of concerns.

