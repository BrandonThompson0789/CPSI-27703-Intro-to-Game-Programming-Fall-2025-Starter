# Level Editor Tooling

The level editor lives under `tools/level_editor` and builds as the `level_editor` executable.

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target level_editor
```

The root CMake target links against the same SDL/Box2D/json dependencies as the main game, so the editor renders scenes consistently with in-game visuals.

## Running

After building, launch the tool from your build directory:

```bash
cd build
./level_editor
```

Assets are automatically copied next to the executable using the `copy_assets_level_editor` target, so the editor can load sprites, sounds, and levels without extra steps.

## Code Map

- `include/level_editor`: public headers (SchemaService, LevelState, ToolSystem, etc.).
- `src`: implementation files and the current `EditorApp`.
- `ui`: placeholder for future ImGui/Qt panels.
- `schemas`: reserved for generated metadata (component descriptors, field definitions).

Each section in `docs/level-editor-design.md` references the corresponding files so an automation agent can implement new features step-by-step.

