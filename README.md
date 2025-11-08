At a glance:

Sprites are loaded from JSON

Input configuration is loaded from JSON

The screen follows and ensures capture of objects with the `ViewGrabComponent`.

Frame Rate is regulated and `deltaTime` is measured for use with other functions.

GIF featuring screen movement as well as debugView with text:
![hippo](docs/project03.gif)

At a longer glance:

# Project03 - SDL2

## Overview
Project03 focuses on the SDL2 layer of the engine. The gameplay loop, rendering path, and input pipeline are all driven by SDL2 subsystems while Box2D provides physics. The goal of this milestone is to demonstrate that sprites render, input devices drive in-game actions, and the camera view reacts to gameplay state.

Key learning objectives covered by this project:
- Apply SDL2’s rendering, window, input, and timing APIs directly.
- Encapsulate SDL2 types inside engine subsystems (`SpriteManager`, `InputManager`, `Engine` view/camera).
- Manage art assets and metadata separately from gameplay code.
- Maintain a clean separation between game state (components, Box2D bodies) and presentation (sprites, view transforms).

## Getting Started
- **Windows setup**: run `setup.bat` once to install vcpkg dependencies, then use `build_release.bat` or open the CMake project in Visual Studio.
- **macOS / Linux**: use `build_release.sh` (requires Ninja + CMake) to pull dependencies and produce the `demo` executable.
- Assets live in `assets/`; build outputs stage everything into `build/win-mingw-debug`, `build/release-x64-mingw-dynamic`, etc., alongside the executable.

To run the demo after building:
```
cd build/win-mingw-debug      # or another configured build folder
.\demo.exe
```

## SDL2 Implementation Highlights
- **Initialization**: `Engine::init()` sets up SDL video, window, renderer, SDL_ttf, and the Box2D debug draw bridge. SDL_Image is used by `SpriteManager`.
- **Rendering**: `SpriteComponent` converts world coordinates to screen space via `Engine::worldToScreen()` before calling `SpriteManager::renderSprite()`; camera scale, translation, and (optional) tiling are all handled in SDL space.
- **Input**: `Engine::processEvents()` feeds the SDL event pump. `InputManager` polls keyboard/gamepad state, supports hot-plug events (`SDL_CONTROLLERDEVICEADDED/REMOVED`), and exposes high-level `GameAction` queries to components.
- **View system**: `ViewGrabComponent` aggregates world bounds for tracked objects each frame and drives the static camera stored in `Engine`. Camera smoothing and scaling keep the gameplay view centered on interesting content.
- **Frame pacing**: `Engine::run()` maintains a nominal target FPS using `SDL_Delay` when the update/render workload finishes early.

## Gameplay Layer
- **Sprites on screen**: `SpriteComponent` pulls frame data from `SpriteManager` and renders animated sprites. Body-driven sprites rotate and scale based on Box2D fixture data or explicit render dimensions.
- **Input-driven actions**: `PlayerMovementComponent` queries `InputComponent` for directional movement, interaction, and throw actions. Keyboard and controller bindings are defined in `assets/input_config.json`.
- **View updates with game state**: Objects containing `ViewGrabComponent` expand the camera’s focus area. The camera follows players/targets smoothly while respecting minimum extents to avoid extreme zoom.

## Asset & Data Pipeline
- **Textures and sprite metadata**: `SpriteManager` loads texture atlases described in `assets/spriteData.json`, caches SDL textures, and exposes lookups via sprite names. Sprites are accessed through a `std::unordered_map`.
- **Level definition**: `Engine::loadFile()` consumes `assets/level1.json`, instantiates objects through `ComponentLibrary`, and seeds Box2D bodies, sprites, and behaviors.
- **Input mapping**: Keyboard and controller bindings are stored in JSON (`assets/input_config.json`, `assets/input_config_arrows.json`). `InputManager` normalizes per-device settings (deadzones, axes vs. buttons).

## Engine Loop & Timing
- **Target FPS**: `Engine::targetFPS` defaults to 60. The main loop measures work time with `SDL_GetTicks()` and yields with `SDL_Delay` if the frame completes early.
- **Delta time usage**: Each frame records the measured elapsed seconds in a static `Engine::deltaTime`, which feeds Box2D stepping and component updates for consistent motion across hardware.

## Directory Layout
```
src/
  Engine.*               # SDL2 bootstrap, main loop, camera handling
  SpriteManager.*        # SDL_Texture cache + sprite atlas lookups
  InputManager.*         # SDL input polling + action mapping
  components/            # Gameplay components (Body, Sprite, ViewGrab, etc.)
  CollisionManager.*     # Box2D contact processing
assets/
  textures/              # PNG spritesheets used by SDL2_image
  spriteData.json        # Sprite atlas metadata (JSON)
  level1.json            # Level definition with component data
  input_config*.json     # Action bindings for keyboard and gamepads
build/                   # Generated build trees (per configuration)
```

## Testing & Debugging Tips
- Launch with `F1` to toggle Box2D debug overlays (requires a `ViewGrabComponent` to align the camera).
- Plug in or disconnect controllers at runtime—`InputManager` listens for the SDL hot-plug events automatically.
- Log output appears on stdout/stderr; errors during asset loading or SDL setup will print diagnostics there.
