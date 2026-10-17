# ThreePoint 3D implementation plan

[README.md](README.md) is the user-provided source of truth. This plan may clarify implementation order, but cannot narrow or replace the 3D, physics, editor, platform, or code-only game requirements. The engine name **ThreePoint** was already chosen in this project, so no naming question is needed.

## Current state and course correction

The existing commits built a **2D SDL3/Lua prototype** with project creation, Lua callbacks, a window, and simple drawing. It is not the requested 3D engine, does not use GLFW/nvrhi/Vulkan, and has no 3D physics. Treat it as exploratory code: retain useful project/Lua tests, then replace or refactor the runtime when the new foundation is ready. No 3D milestone below is complete.

The core will be a C++ static library with separate game runtime and editor executables. Projects remain source files: `game.lua`, literal `scenes/*.scene.lua` data, `scripts/*.lua`, and referenced assets. The editor edits the same files; it must not introduce hidden game state. `threepoint run`, `test`, and `build` must work without opening the editor. Exported games include a runtime without editing tools. Lua remains the scripting language. Choose and pin one 3D physics dependency during its integration phase; **Jolt Physics** is the initial choice.

## Small, testable phases

Phase 1 creates `tests/feature_scene/` and its coverage manifest. Every implementation phase adds relevant unit or integration tests, runs them, and extends that scene for each new component and Lua API. Never claim a phase complete until its check passes. Keep the feature scene runnable from source once the 3D Lua runtime exists and, once export exists, from a packaged build.

**Phase 1 status (2026-10-17):** The static core, separate runtime/editor targets, optional legacy prototype, source-only feature-scene skeleton, dependency record, and Windows/macOS/Ubuntu CI workflow are implemented. Local core and legacy checks pass. The phase gate remains open until all three CI jobs actually run; this repository has no Git remote configured, so those results are not available yet.

| Phase | Deliverable | Check before commit |
| --- | --- | --- |
| 0 | Restore this 3D brief, this roadmap, `AGENTS.md`, and project-local development skills. | Requirements for every item in README map to a phase; no 2D plan is presented as the goal. |
| 1 | Restructure CMake into a static core library and separate runtime/editor targets; pin dependencies, create the feature-scene skeleton, and start Windows, macOS, Ubuntu 24+ CI. | All targets compile on the three platforms; core unit-test target runs. |
| 2 | Replace the SDL window path with GLFW, Vulkan, and nvrhi device/swapchain setup. | Runtime opens, clears, resizes, and closes on each platform; device-loss and missing-Vulkan errors are clear. |
| 3 | Add glm transforms, depth testing, a perspective camera, and a built-in cube mesh. | **First visible 3D demo:** a cube rotates with correct depth in an automated frame capture and a manual window check. |
| 4 | Connect Lua projects to the 3D scene/camera/render path and a repeatable fixed/update/draw loop. | **First code-only 3D demo:** a Lua scene creates and animates the cube with no editor action. |
| 5 | Add keyboard, mouse, and controller actions plus pointer-to-world ray creation. | Injected events drive the cube/camera predictably in tests. |
| 6 | Add stable entity IDs, transforms, cameras, lights, mesh renderers, and script components in a small ECS. | Create, change, query, and destroy each component from Lua and in core tests. |
| 7 | Add source-defined scenes and reusable prefabs, including runtime spawn, destroy, and scene switch. | Reloading a scene and spawning a prefab reproduce the same entities with no editor database. |
| 8 | Import glTF geometry, node transforms, and scene hierarchy. | A checked-in glTF fixture renders in the feature scene; bad asset errors name the file. |
| 9 | Import glTF textures/materials and implement direct-light PBR. | Reference material spheres and textured glTF fixtures pass image checks. |
| 10 | Render into an HDR target and add exposure and tonemapping. | Bright highlights retain detail; exposure changes are measurable in captured frames. |
| 11 | Add image-based lighting from project HDRIs, including a documented Poly Haven HDRI example. | A PBR fixture responds to the HDRI, and missing/invalid HDRIs fail clearly. |
| 12 | Add soft shadow maps for the supported light types. | Occluder tests show stable, softened shadows across camera movement. |
| 13 | Add SSAO with adjustable quality and a disable path. | A corner/contact fixture darkens appropriately without large halos or temporal flicker. |
| 14 | Integrate pinned Jolt Physics with rigid bodies, colliders, triggers, and fixed-step simulation. | Falling, resting, collision, and trigger cases pass deterministic core tests. |
| 15 | Expose physics creation, forces, motion, queries, and contact events to Lua and scene components. | The feature scene drives and inspects physics entirely from Lua. |
| 16 | Add miniaudio playback, spatial sound, and script control. | Automated audio-state tests pass; the feature scene plays a 3D sound on contact. |
| 17 | Add script-defined UI, timers, and save/load needed by complete games. | A source-only menu starts play and a save restores the same state. |
| 18 | Build the editor scene tree and component inspector against project source files. | A human can open, create, and change entities; saved files run identically from CLI. |
| 19 | Add viewport selection and transform gizmos. | Moving an entity with the gizmo writes the expected scene transform and survives reopen. |
| 20 | Add editor script/source view and round-trip checks; expose any editor action through files or CLI. | Hand edit, editor edit, save, and reopen preserve behavior; an agent can author the same result without clicks. |
| 21 | Implement `threepoint test` and `threepoint build` to run tests headlessly and export a standalone, non-editable game. | A small source-only fixture exports and runs without the installed editor or development files. |
| 22 | Build a complete 3D falling-block game from Lua and source assets only. | A match can finish with `threepoint run`; rule tests and export smoke test pass. |
| 23 | Build a small 3D room/puzzle game from Lua and source assets only. | It exercises glTF, PBR, HDRI, shadows, SSAO, physics, audio, UI, save/load, and export. |
| 24 | Harden and release: automated feature-scene coverage audit, sanitizers, asset-failure cases, performance budgets, platform packaging, and documentation. | Windows, macOS, and Ubuntu 24+ release builds pass unit, integration, visual, packaged-game, and stability gates. |

Phase 3 is the first 3D visual milestone; Phase 4 is the first 3D demo authored wholly in Lua. Work may be split into smaller commits if a check proves too large, but do not skip tests or silently defer a requirement.

| README requirement | Covered in phases |
| --- | --- |
| Windows, macOS, Ubuntu 24+, production testing, all-feature scene | 0–1, every feature phase, 24 |
| C++/CMake static core, runtime/editor, GLFW, nvrhi, Vulkan, glm | 1–4, 18–20 |
| miniaudio and Lua behavior | 4, 6–7, 16 |
| AI-authored source-only games, human editor, distributable export | 4, 7, 18–23 |
| glTF, materials/textures, PBR, HDRI/IBL, shadows, SSAO, HDR tonemapping | 8–13 |
| Scriptable, component-authored 3D physics | 14–15 |
| Entity/component control, update loop, spawn/destroy, prefabs | 4, 6–7 |

## 3D Lua contract to complete and test

`game.lua` declares window settings, input actions, and scenes. Each scene table describes entities and components using project-relative asset paths. Behavior scripts return `start`, `fixed_update(dt)`, `update(dt)`, and optional render callbacks. The public `require("threepoint")` API must cover:

| Area | Required script capability |
| --- | --- |
| Lifecycle and scenes | Quit, switch/reload scene, inspect active scene, deterministic time. |
| Entities and prefabs | Spawn, destroy, find, enumerate, and instantiate prefabs with stable IDs. |
| Components and transforms | Add/get/remove components; position, rotation, scale, parenting, visibility. |
| Camera and input | Perspective/orthographic camera, screen ray, named keyboard/mouse/controller actions. |
| Rendering and assets | Mesh/glTF load, material and texture assignment, lights, environment HDRI, quality/exposure settings. |
| Physics | Body/collider/trigger creation, forces and velocity, ray/shape queries, contact callbacks. |
| Audio | Play, stop, volume, spatial source and listener. |
| UI and game flow | Text, buttons, focus, timers, save/load. |
| Testing | Start/reset a scene, inject input, step time, inspect entities/physics/UI, compare captures. |

For each public call, add a runnable Lua example, invalid-input behavior, and an automated test in the feature scene. The editor must be able to author the same component values, while handwritten scripts can create or change them at runtime.

**Sample design check:** The falling-block game uses prefabs for 3D cubes, named input, timed movement, scene spawning/destruction, UI score, audio, and save data. The room/puzzle game uses glTF meshes, PBR materials, an HDRI, lights/shadows/SSAO/HDR, physics triggers, Lua contact logic, spatial audio, UI, and scene switching. Every named capability appears above; if implementation reveals a missing operation, add it to this checklist and test it before building an editor control for it.

## Release rules

- Keep unit tests for math, serialization, ECS, importers, and physics; integration tests for Lua and assets; visual reference checks for rendering; and packaged-game smoke tests on every supported OS.
- Update `tests/feature_scene/` whenever a component or scripting API is added. Its manifest lists every feature and its test so coverage can be audited.
- A release must build from a clean checkout using pinned dependencies, export both code-only sample games, and run them without editor clicks or manual asset placement.
- Record implementation status honestly. Source files in the current 2D prototype are not evidence that a 3D phase passed.
