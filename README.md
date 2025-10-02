# Project01 - Object Factory

A C++ game engine project demonstrating object-oriented design patterns, JSON-based level loading, and SDL2 graphics rendering. This project implements an object factory pattern to dynamically create game objects from JSON configuration files.

## Overview

This project showcases a modular game engine architecture with:
- **Object Factory Pattern**: Dynamic object creation using a factory registry
- **JSON Level Loading**: Data-driven level design with JSON configuration
- **Polymorphic Object System**: Base object class with specialized derived types
- **SDL2 Integration**: Hardware-accelerated 2D graphics and input handling

## Level Data (JSON)

The game uses JSON files to define level layouts and object configurations. The level data is stored in `assets/level1.json` and contains:

### Structure
```json
{
  "player": { ... },
  "enemies": [ ... ],
  "items": [ ... ]
}
```

### Current Level Configuration
- **Player**: Single player character positioned at (50, 500) with 32x32 size
- **Enemies**: 3 enemy entities with varying health (30-75), damage (10-20), and sizes (20x20 to 28x28)
- **Items**: 4 collectible items including:
  - 2 coins (value: 10, 25)
  - 1 health potion (value: 50)
  - 1 power-up (value: 100)

Each object includes position coordinates, size dimensions, and type-specific properties like health, damage, item type, and value.

## Object Types

The game implements a hierarchical object system with polymorphic behavior:

### Base Object Class (`Object.h/cpp`)
- **Abstract base class** defining the common interface
- **Core properties**: position (Vector2), size (Vector2), angle (float)
- **Virtual methods**: `update()`, `render()`, property getters/setters
- **Common data structure**: Uses Vector2 from `Common.h` for 2D coordinates

### Player (`Player.h/cpp`)
- **Inherits from Object**
- **Additional properties**: velocity (Vector2), speed (int)
- **Functionality**: Player movement and input handling
- **JSON constructor**: Initializes from level data
- **Controls**: WASD/Arrow keys for movement, Space for jump, Escape to quit

### Enemy (`Enemy.h/cpp`)
- **Inherits from Object**
- **Additional properties**: health (int), damage (int)
- **Functionality**: Enemy behavior and combat stats
- **JSON constructor**: Loads health and damage from level data
- **Rendering**: Red rectangles to distinguish from other objects

### Item (`Item.h/cpp`)
- **Inherits from Object**
- **Additional properties**: itemType (string), value (int)
- **Functionality**: Collectible items with different types and values
- **JSON constructor**: Loads type and value from level data
- **Types supported**: "coin", "health_potion", "power_up"

## Library System (`Library.h`)

The Library implements the **Factory Pattern** for dynamic object creation:

### Factory Registry
- **Singleton pattern**: Global instance accessible via `getLibrary()`
- **Function map**: Maps string keys to factory functions
- **Lambda factories**: Each object type registered with a lambda function
- **JSON integration**: Factory functions accept JSON data for object initialization

### Registered Types
```cpp
map["player"] = [](const nlohmann::json& data) -> std::unique_ptr<Object>
map["enemy"] = [](const nlohmann::json& data) -> std::unique_ptr<Object>
map["item"] = [](const nlohmann::json& data) -> std::unique_ptr<Object>
```

### Benefits
- **Extensibility**: Easy to add new object types
- **Data-driven**: Objects created from JSON without hardcoded logic
- **Type safety**: Returns polymorphic Object pointers
- **Memory management**: Uses smart pointers for automatic cleanup

## Engine System (`Engine.h/cpp`)

The Engine serves as the core game loop and system manager:

### Core Responsibilities
- **SDL2 Management**: Window creation, renderer setup, cleanup
- **Game Loop**: 60 FPS game loop with event processing, update, and render phases
- **Object Management**: Container for all game objects with polymorphic operations
- **Input Handling**: Keyboard state tracking and key mapping
- **Level Loading**: JSON file parsing and object instantiation via Library

### Key Features
- **Screen Resolution**: 800x600 window with hardware acceleration
- **Input System**: Static key state map accessible to all objects
- **Object Container**: Vector of unique_ptr<Object> for polymorphic storage
- **Error Handling**: Comprehensive SDL and JSON error checking
- **Frame Rate**: Consistent 60 FPS with SDL_Delay timing

### Supported Controls
- **Movement**: A/D or Left/Right arrows (horizontal), W/S or Up/Down arrows (vertical)
- **Action**: Space bar for jump
- **System**: Escape key to quit

### Loading Process
1. Parse JSON file using nlohmann::json library
2. Clear existing objects from previous levels
3. Create player object using Library factory
4. Iterate through enemies array, creating each via factory
5. Iterate through items array, creating each via factory
6. Store all objects in polymorphic container for unified processing

## Main Entry Point (`main.cpp`)

The main function demonstrates the engine's simplicity and ease of use:

### Execution Flow
```cpp
1. Create Engine instance
2. Initialize SDL systems and create window/renderer
3. Load level data from "assets/level1.json"
4. Run main game loop until quit
5. Cleanup SDL resources and exit
```

### Design Philosophy
- **Minimal main**: All complexity encapsulated in Engine class
- **Single responsibility**: Main only handles high-level flow
- **Error handling**: Engine methods handle all error conditions
- **Resource management**: RAII ensures proper cleanup

## Technical Dependencies

- **SDL2**: Graphics, input, and window management
- **nlohmann/json**: Modern C++ JSON parsing library
- **C++14**: Smart pointers, lambda functions, auto keyword
- **CMake**: Cross-platform build system
- **vcpkg**: Package management for dependencies

## Build and Run

The project uses CMake with vcpkg for dependency management. The executable loads `assets/level1.json` on startup and creates a game world with the configured objects.

```bash
# Build (assuming CMake setup)
cmake --build build

# Run
./build/demo.exe
```

The game window will display all objects as colored rectangles: blue for player, red for enemies, and green for items, positioned according to the JSON level data.
