# Level Editor Design Document

**Purpose**  
Plan and track development of a standalone level editor that loads, visualizes, modifies, and saves `assets/levels/*.json` files for this game while minimizing duplicate logic and ensuring an editor UX that mirrors in-game visuals.

---

## Source Data Inventory

- `assets/objectData.json`: master list of templates, names, component defaults, and sprites. Drives palette thumbnails and component schema seeds.
- `assets/spriteData.json`: sprite sheet atlas including per-texture sprite metadata used for preview renders, tiled backgrounds, and anchors.
- `assets/soundData.json`: mixer defaults plus named sound collections referenced by `SoundComponent` and level-wide sound lists.
- `assets/levels/*.json`: user-authored levels (title, order, thumbnails, background layers, objects). Editor will read/write these files.
- `assets/*.json` (input configs, physics materials, etc.) may be referenced indirectly and should be loadable if components expose those keys.

**Actionable takeaway:** build a schema service that digests the above JSON once, emits typed maps (key → data type + constraints), and exposes helper methods so UI code never needs ad-hoc key/value knowledge.

---

## Functional & UX Requirements

1. Separate desktop tool (can be SDL/ImGui, Qt, or similar) sharing the runtime renderer/physics helpers so the scene view matches in-game visuals.
2. Top-left toolbar containing `New`, `Load`, `Save` plus scrollable grid of object templates (thumbnail + label) sourced from `objectData.json` and supporting right-click edit.
3. Additional toolbar buttons open modal/editor panes:
   - Background layers & parameters
   - Level metadata (name/order/thumbnail, spawn info, win conditions, etc.)
   - Sprite manager (view/add/remove/edit)
   - Sound manager (view/add/remove/edit collections & assets)
4. Central viewport mirrors in-game camera, supports:
   - WASD or right-click drag for panning when no selection
   - Mouse wheel or +/- zoom with min/max bounds
   - Accurate rendering of sprites, parallax backgrounds, collision overlays
5. Object interactions:
   - Left-click to select/highlight objects.
   - Right-click a highlighted object to open the tool picker (Move/Rotate, Resize). Hotkeys (`M/R`) are alternate activators, and `Esc` always returns to Selection mode.
   - Hold `Shift` while left-clicking to temporarily enter placement mode and drop the active template under the cursor.
   - Right-click context menu: `Move/Rotate`, `Resize`, `Edit Components`
   - Move/Rotate exposes translation handles + rotation gizmo anchored to object
   - Resize shows per-side/per-corner stretch handles
   - Edit Components opens dedicated panel (see below)
6. Component editor:
   - Scrollable list of components with collapse/expand, delete action, add button at bottom
   - Sensor, Rail, Joint, ObjectSpawner components need integrated map-picking UX for coordinate parameters while still allowing numeric entry
7. Persistency:
   - `New` seeds blank level (pre-populated metadata)
   - `Load` selects existing level JSON and hydrates state
   - `Save` writes JSON with deterministic ordering & validation (w/ dirty-state guard)
8. UX quality:
   - Undo/redo stack per action
   - Visual cues (hover, selection, error states)
   - No duplicated rendering/business logic: reuse shared modules where possible

---

## Data Schema Strategy

- **Schema ingestion:** On startup, read `objectData.json`, `spriteData.json`, `soundData.json`, `assets/levels/*.json` to build:
  - Template catalog: `templateId → { displayName, defaultComponents, previewSprite }`
  - Component schema map: `componentType → { field, type, enum, min/max }` (derive from existing data + manual schema definitions where dynamic)
  - Level schema: strongly-typed model describing `background.layers`, `objects[*].components[*]`, metadata, etc.
- **Type safety:** Represent schema as typed descriptors (e.g., `FloatField`, `Vector2Field`, `EnumField`, `SpriteRef`, `SoundCollectionRef`). This fuels auto-generated forms and validation.
- **Diff-friendly save:** Always maintain consistent field ordering and drop derived/transient properties before serialization to avoid noisy diffs.
- **Extensibility:** Schema map should be declarative (JSON/YAML or code) so adding components requires no UI rewrite.
- **Full coverage requirement:** Existing JSON files do not showcase every possible key/value pair. The schema layer must therefore scan relevant engine code (e.g., component classes, serializer helpers) or consume dedicated metadata to enumerate every supported property so designers can edit fields that currently only exist in code.
- **Automated scanner:** `SchemaScanner` walks `src/components/**/*Component.cpp`, extracts `json.value(...)`, `json["..."]` and related usages to build a superset of keys plus inferred value types. Its output augments the base schema map so UI auto-forms know about fields that never appear in shipped JSON files.
- **Validation helpers:** `SchemaValidator` consumes the merged `SchemaCatalog`, enforces required fields, type constraints, enum membership, and emits structured issues for UI badges/tooltips.

---

## Application Architecture

- **Core Modules**
  1. `SchemaService`: parses JSON sources, normalizes schema descriptors, exposes query helpers.
  2. `AssetCache`: loads textures/sprites/sounds for previews; abstracts renderer-level handles.
  3. `LevelState`: in-memory representation of the level, tracks selection, dirty status, undo stack.
  4. `ViewportController`: handles camera transforms, input binding (WASD, drag, zoom), render pipeline.
  5. `ToolSystem`: pluggable set of manipulators (selection, move, rotate, scale, component edit) sharing gizmo rendering utilities.
  6. `ModalManager`: orchestrates toolbar popups (background editor, metadata, sprite/sound managers, component editor).
  7. `FileService`: handles `New/Load/Save`, file dialogs, JSON validation, error surfacing.
- **Tech considerations**
  - Reuse existing rendering code by linking against game engine modules where possible to avoid reimplementing sprite batching, Box2D debug draw, etc.
  - For UI, consider ImGui (fits SDL-based tool) or Qt (if native widgets preferred). Ensure docking layout supports viewport + sidebar arrangement.

---

## Code Structure & Automation Guidance

To let an AI agent progress through each milestone with minimal clarification, establish a predictable file layout, predeclare the main interfaces, and provide per-step coding cues.

### Directory & Build Layout

- Create `tools/level_editor/` (sibling to `src/`) containing:
  - `include/` for public headers.
  - `src/` for implementation files.
  - `ui/` for panels/layout helpers, ImGui/Qt utilities, icons, and styles.
  - `schemas/` for optional JSON/YAML descriptors defining component metadata.
- Add a `level_editor` executable target in the root `CMakeLists.txt`, linking against existing engine libraries (rendering, physics, serialization) plus UI dependencies (SDL, ImGui, Qt, etc.).
- Document build/run commands inside `tools/level_editor/README.md` so automation agents don’t need to ask how to build or launch the tool.

### Core Class Skeletons

Create stub headers/implementations early so each later step has defined extension points:

- `SchemaService.h/.cpp`: static `LoadAll`, `GetTemplate(id)`, `ListComponents()`, `GetComponentDescriptor(type)`.
- `AssetCache.h/.cpp`: sprite preview helpers (`GetSpriteThumb`, `PrimeTexture`), sound preview hooks.
- `LevelState.h/.cpp`: holds `LevelDocument`, selection state, dirty flag, undo stack operations (`PushCommand`, `Undo`, `Redo`).
- `ViewportController.h/.cpp`: owns `Camera`, handles `HandleInput`, `Render`, `SetTool`.
- `ToolSystem/Tool.h`: base `ITool` plus derived `SelectionTool`, `MoveRotateTool`, `ResizeTool`, `ComponentEditTool`.
- `ModalManager.h/.cpp`: registers modal factories, exposes `OpenModal(ModalId, ModalContext)`, and routes callbacks into `LevelState`.
- `FileService.h/.cpp`: implements `NewLevel`, `LoadLevel(path)`, `SaveLevel(path)`, `ConfirmSaveIfDirty`.

### Step-by-Step Coding Prompts

| Step | Primary Files | Interfaces to Touch | Automation Notes |
|------|---------------|--------------------|------------------|
| 0 | `docs/level-editor-design.md`, `tools/level_editor/README.md` | N/A | Capture tech stack decisions, dependency list, and environment setup script. |
| 1 | `SchemaService.*`, `schemas/*.json`, optional `schema_dump.cpp` | `SchemaCatalog`, `ComponentDescriptor` | Include scanners for `src/components/*`/`ObjectSerializer*` to discover fields missing from JSON; expose CLI `--schema-dump` to verify. |
| 2 | `tools/level_editor/src/main.cpp`, `FileService.*`, `LevelState.*` | `EditorApp`, `Command` | Implement the command pattern base now so undo/redo wiring exists before tool work begins. |
| 3 | `ViewportController.*`, `ToolSystem/SelectionTool.*`, `CameraController.*` | `CameraState`, `ViewportInput` | Keep camera math in standalone helpers with unit tests for unattended validation. |
| 4 | `ui/SidebarPanel.*`, `TemplatePalette.*` | `TemplateViewModel`, `PaletteController` | Provide event bus/notifier so palette refreshes after schema updates; document JSON → view-model conversion. |
| 5 | `ToolSystem/MoveRotateTool.*`, `ToolSystem/ResizeTool.*`, `GizmoMath.*` | `GizmoState`, `ManipulationContext` | Centralize snapping/angle math separate from rendering; reuse handles across tools. |
| 6 | `ui/ComponentEditorPanel.*`, `SpecialComponentHandlers/*`, `MapInteractionSession.*` | `ComponentForm`, `CoordinatePicker` | Define `IMapInteractionSession` so Sensor/Rail/Joint/ObjectSpawner pickers share one contract. |
| 7 | `ui/BackgroundModal.*`, `LevelParamsModal.*`, `SpriteManagerPanel.*`, `SoundManagerPanel.*` | `LayerViewModel`, `SpriteAsset`, `SoundCollection` | Ensure modals dispatch mutations through `LevelState::ApplyChange` for undo integration. |
| 8 | `SettingsPanel.*`, `ValidationOverlay.*`, packaging scripts | `EditorSettings`, `ValidationResult` | Add regression script (`tools/level_editor/tests/run_regression.py`) to load/save sample levels automatically. |

### Automation Tips

- Document public methods with concise comments describing intent and side effects.
- Tag TODOs with the associated step (`// TODO(step5): finish rotate gizmo visuals`) so agents know whether they should act on them.
- Favor constructor injection for dependencies (`SchemaService&`, `AssetCache&`) to keep modules mockable for automated tests.
- After each step, add a diagnostic command or unit test (e.g., `level_editor --verify-level assets/levels/level1.json`) so automated runs can confirm success without manual inspection.

---

## UI Layout & Interaction Blueprint

- **Main Frame**
  - Left sidebar (fixed width): grouped sections.
    1. Toolbar row with `New`, `Load`, `Save`.
    2. Scrollable template grid (thumbnail + label). Supports drag-to-place and right-click modify.
    3. Button cluster for modals: `Backgrounds`, `Level Params`, `Sprites`, `Sounds`.
  - Center viewport: actual level rendering with overlays for selected objects, grid, gizmos.
  - Optional right-side inspector that mirrors context menu actions for accessibility.
- **Template Grid**
  - Data binding to template catalog; search/filter bar for quick lookup.
  - Each tile shows sprite preview (render from `spriteData.json` frames) and template name from `objectData.json`.
  - Right-click options: `Edit Template`, `Duplicate`, `Remove` (if allowed).
- **Modal Editors**
  - Background editor: stacked list of layers with reorder drag handles, sprite picker, parallax/offset/tiling controls, live preview overlay toggles.
  - Level params: text fields for name, order, optional metadata, validation for uniqueness/order collisions.
  - Sprite manager: tree view by texture → sprite; supports frame editing, dimension inputs, import pipeline.
  - Sound manager: list of collections referencing actual `.wav` assets; playback preview.

---

## Editing Workflows

1. **Placement**
   - Drag template from grid into viewport or click tile to enter "placement mode" then click in viewport.
   - Snap-to-grid toggle and adjustable grid size for precise placement.
   - On placement, instantiate object with template defaults + pointer to schema map for subsequent edits.
2. **Selection**
   - Left-click object; shift-click toggles multi-select (if needed).
   - Outline + bounding box indicates selection; overlay shows template name & components summary.
3. **Move/Rotate Tool**
   - Activates translation arrows + central pivot dot; right-click menu entry toggles this state.
   - Rotation ring or handle at offset; show live angle readout.
4. **Resize Tool**
   - Display handles at corners/sides; enforce component constraints (e.g., BodyComponent fixtures) via schema descriptors.
   - Optionally show size overlay in pixels/meters.
5. **Component Editor**
   - Panel lists each component with accordions; editing fields auto-generated from schema types.
   - Delete button per component; `+ Add Component` opens searchable list limited by schema.
   - Special components (Sensor, Rail, Joint, ObjectSpawner) integrate map picker for coordinates: click `Pick` to focus viewport, place handles, confirm to write values.
6. **Special Data Interaction**
   - Sensor: define shape/radius/flags; map overlay shows area of effect.
   - Rail: polyline editor (add/move points) directly in viewport.
   - Joint: choose two objects, display possible anchors, provide constraint sliders.
   - ObjectSpawner: timeline or spawn rule editor plus coordinate selection.

---

## Persistence & Validation

- **New**: resets `LevelState` to default, optionally prompting to save if dirty.
- **Load**: open dialog filter to `assets/levels/*.json`, parse via schema, surface errors with actionable messaging.
- **Save**: run validation pipeline (schema compliance, reference integrity, bounding boxes, asset existence). Warn/stop on critical failures.
- **Validation pipeline:** `FileService` now invokes `SchemaValidator` when loading/saving, returning structured issues so the UI can badge problematic objects while still allowing partial edits.
- **Viewport progress:** The SDL-powered viewport renders parallax background layers from `background.layers`, sprite textures sourced from `spriteData.json`, grid overlays, and selection outlines, using the same sprite metadata as the runtime for fidelity.
- **Toolbar progress:** Left sidebar now lives inside the SDL window with `New/Load/Save` actions wired to `FileService`, plus a schema-driven template palette that highlights the active template and feeds the placement tool for click-to-place object creation.
- **File choosing:** A lightweight in-editor file dialog lists `assets/levels/*.json` files so Load now lets designers pick any level without leaving the app. Save defaults to the current file (or an autosave slot) to mirror that workflow.
- **JSON Integrity**: maintain comment preservation if needed (otherwise, drop). Provide pretty-printed output matching existing style.
- **Dirty Tracking**: LevelState tracks modifications per object/component; UI indicates unsaved changes.

---

## Implementation Plan & Checklist

Each step has deliverables that can be checked off as work completes. Keep boxes unchecked `[ ]` until the milestone is done; swap to `[x]` when finished.

### Step 0 – Confirm Requirements & Tooling
- [ ] Review this document with stakeholders; adjust scope, priorities, and UI expectations.
- [ ] Decide on UI tech stack (ImGui + SDL reuse vs. Qt vs. web/electron) and rendering integration strategy.

### Step 1 – Schema & Data Services
- [x] Implement `SchemaService` that ingests `objectData.json`, `spriteData.json`, `soundData.json`, and produces normalized descriptors.
- [x] Create validation helpers (type checking, constraint enforcement, reference resolution).
- [x] Expose API for fetching template metadata, sprite previews, and sound collections.
- [x] Scan component/serializer code to capture fields not represented in JSON samples so the schema map covers all valid keys and value types.

### Step 2 – Application Shell & File I/O
- [x] Scaffold standalone editor application with windowing, menu loop, and basic event handling.
- [x] Implement `FileService` for `New/Load/Save`, including JSON serialization/deserialization and dirty-state prompts.
- [x] Add undo/redo infrastructure in `LevelState`.

### Step 3 – Viewport & Camera Controls
- [x] Integrate renderer/engine modules to display level background + objects accurately.
- [x] Implement camera controls: WASD/right-click drag for pan, mouse wheel +/- for zoom with smoothing and bounds.
- [x] Add selection overlay basics (bounding boxes, highlight shader).

### Step 4 – Toolbar & Template Palette
- [x] Build left sidebar with `New/Load/Save` buttons and template grid populated from `SchemaService`.
- [x] Support drag-and-drop placement, click-to-place mode, and right-click template actions. *(Drag preview reflects camera/zoom, reapply shortcut lives in the Apply button, and right-click now opens the template editor.)*
- [x] Hook template edits back into schema/templates, ensuring updates propagate to existing instances if desired. *(Modal editor writes back to `objectData.json`, reloads schemas, and offers an "apply to existing objects" checkbox.)*

**Next Up:** add resize handles (respect schema bounds/aspect rules) and route tool activation through a shared controller so future tools (resize, component edit) can plug in cleanly.

### Step 5 – Object Manipulation Tools
- [x] Implement move/rotate gizmo with snapping, pivot control, and on-canvas feedback. *(M toggles move mode; translation plus rotation anchor are fully interactive with 15° snapping.)*
- [x] Implement resize handles honoring schema constraints (min/max, maintain aspect where required).
- [x] Centralize tool activation via `ToolSystem` to avoid duplicated input handling.

### Step 6 – Component Editor & Special Components
- [x] Create component list UI with autogenerated forms per component schema.
- [x] Implement add/remove flows, including template inheritance vs. overrides visualization.
- [x] Build special interaction modes for Sensor, Rail, Joint, ObjectSpawner with viewport picking + manual entry sync.

### Step 7 – Background, Level, Sprite, Sound Editors
- [ ] Background modal: layer list, reorder, sprite picker, tiling/parallax controls with live preview.
- [ ] Level parameters modal: metadata fields, validation (name uniqueness/order), thumbnail management.
- [ ] Sprite manager: texture import, sprite frame editing, integration with template previews.
- [ ] Sound manager: collection CRUD, waveform preview, linking to components.

### Step 8 – Polish, Testing, & Packaging
- [ ] Add global settings (grid snap, units, theme), status bar, and contextual help/tooltips.
- [ ] Implement validation overlays/error badges for invalid objects/components.
- [ ] Run regression tests: load/save existing levels (`level1.json`, `level2.json`, etc.), confirm no diffs unless intentional.
- [ ] Package distributable build alongside documentation for designers.

---

## Testing Strategy

- Unit tests for schema parsing, validation rules, and JSON roundtrips.
- Snapshot/render tests comparing editor viewport frames to in-game captures for key scenarios.
- Interaction tests (manual/automated) covering placement, component edits, special component workflows, and asset editor dialogs.
- Smoke tests on all reference levels before releases to ensure compatibility.

---

## Future Enhancements (Post-MVP Ideas)

- Collaborative editing or shared project files.
- Procedural placement tools (brush/paint, scatter).
- Alignment guides, measurement tools, and physics simulation playback inside the editor.
- Plugin system so designers can script custom inspectors without modifying core code.

---

**Tracking Usage**  
Keep this document updated as tasks complete. Consider embedding it into project management tooling (convert checkboxes into tickets) to maintain visibility.

