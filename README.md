# AI Game Engine

We're building a production-grade, simple and straight-forward 3D Game Engine for Windows, macOS and Linux (Ubuntu 24+). This is professional-grade software that needs to be stable, fully-tested, and real-world deployable. No hacks, no shortcuts - solid as a rock.

## Dev Workflow
- Ask questions only when absolutely necessary - work autonomously
- IMRORTANT! DO NOT look outside of working directory or use other code on my computer as reference
- Start by asking question to determine engine name. Suggest a few
- Testing is of the utmost importance - use lots of unit tests (any framework, simple is good), run automated testing
- Setup a scene used for testing, which tests every single feature in the engine - all components, and entire scripting API
- Do things properly, this is production-grade and not a hack project
- Create AGENTS.md and relevant skills to support development, and contain concrete development guidelines

## Tech stack
- C++ and CMake for core engine
- [GLFW](https://github.com/glfw/glfw) and [nvrhi](https://github.com/NVIDIA-RTX/NVRHI), using Vulkan primarily on all platforms
- [glm](https://github.com/g-truc/glm) for math
- [miniaudio](https://github.com/mackron/miniaudio) for audio
- Lua for scripting, though this can be up for discussion

## Basic Architecture
- Static library for core engine, executable for editor and runtime (depending on chosen design)
- Need to have editor to build games - this can be embedded into runtime executable (and stripped from distribution builds), or can be a standalone executable
- Simple ECS perhaps, to author scene with entities and components
- Ability to "export" game - executable that runs game without editing ability that we can distribute
- Editor needs to be fully controllable by AI agents - I should be able to ask you to build me a game like Tetris, and you should have all the tools available to do so without my intervention

## 3D Renderer
- Import gltf meshes with materials and textures
- Editor has gizmo to position them in the world
- PBR material workflow
- IBL with HDRIs from https://polyhaven.com/hdris
- Soft shadow maps
- Good SSAO
- HDR pipeline with tonemapping

## 3D Physics
- Use simple 3D physics library of your choosing
- Controllable via scripting, authored via components in editor

## Behavior
- Scripting with Lua (or other chosen language)
- Control entities/components and run in update loop, and entity destruction/creation
- Spawn new entities/prefabs

## Implementation plan

The requirements above are the source of truth. [PLAN.md](PLAN.md) maps each requirement to small, testable phases and records the current state. The existing SDL3/Lua program is a 2D prototype; it does not satisfy the 3D renderer or physics requirements. The first visible 3D demo is complete in Phase 3, and the first Lua-authored 3D demo is complete in Phase 4.

## Current implementation

**Platform scope update (2026-10-17):** The current target is Linux (Ubuntu 24+). Windows and macOS are no longer implementation or release gates, per the latest project direction. The original brief above is retained as supplied; this update supersedes only its platform list.

The C++ static core, separate runtime/editor targets, source-only feature scene, dependency record, and Linux CI workflow are in the repository. The runtime uses glm transforms, an indexed cube mesh, shaders, and depth-tested Vulkan rendering through nvrhi. A pinned Lua 5.4.8 runtime runs the feature scene, which creates and animates a 3D cube entirely through source files. Phase 5 adds named keyboard, mouse, and controller actions, pointer rays, and Lua camera control. The old 2D SDL3/Lua program is available as the optional `threepoint-2d-prototype` target. All twelve local Release tests pass, including GPU captures under Vulkan validation; hosted Ubuntu 24+ CI remains unverified until the workflow runs.

On this Ubuntu development machine, `bash scripts/bootstrap-local.sh` builds both targets and runs all available tests. Run `build/threepoint run tests/feature_scene` to see the source-only Lua cube game. For a reproducible 60th-frame capture, run `build/threepoint run tests/feature_scene --frames 60 --capture build/lua-cube.ppm`. `build/threepoint --preview` still shows the built-in rotating cube, and `--capture-clear` keeps the Phase 2 clear-frame check. No editor action is needed to run the Lua scene.

The first 3D Lua API is `local tp = require("threepoint")`. In `start()`, call `tp.spawn({mesh="builtin:cube", position={0,0,0}, rotation={0,0,0}, scale={1,1,1}})` to get a stable cube ID. In `update(dt)`, call `tp.set_rotation(id, {x,y,z})` using radians, or read deterministic elapsed time with `tp.time()`. `tp.quit()` ends the run. Scripts return `start`, `fixed_update(dt)`, `update(dt)`, and optional `draw` callbacks. See [the runnable feature-scene script](tests/feature_scene/scripts/main.lua). Invalid mesh names, nonfinite vectors, nonpositive scales, unknown IDs, and wrong argument counts raise Lua errors with callback context.

Declare named actions in `game.lua` under `input`, for example `turn = {keyboard="space", gamepad_button="a"}`. Supported controls are letters/digits and common navigation keys, left/right/middle mouse buttons, standard GLFW gamepad buttons, and left/right stick or trigger axes on the first connected gamepad. `tp.action("turn")` returns 0 when idle, 1 for pressed buttons, or a value from -1 to 1 for axes; if an action has multiple bindings, the strongest magnitude wins. `tp.pointer()` returns window coordinates. `tp.screen_ray(x,y)` returns `{origin={x,y,z}, direction={x,y,z}}` in world space. `tp.set_camera({eye={x,y,z}, target={x,y,z}, up={x,y,z}, fov=60, near=0.1, far=100})` changes the perspective camera; omitted fields keep their prior values. The [feature-scene script](tests/feature_scene/scripts/main.lua) exercises these calls. For an automated held-key capture, run `build/threepoint run tests/feature_scene --frames 60 --capture build/held.ppm --hold-key space`. General components, assets, physics, and export remain subsequent phases.
