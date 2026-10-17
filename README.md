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

The requirements above are the source of truth. [PLAN.md](PLAN.md) maps each requirement to small, testable phases and records the current state. The existing SDL3/Lua program is a 2D prototype; it does not satisfy the 3D renderer or physics requirements. The first visible 3D demo is planned for Phase 3, and the first Lua-authored 3D demo for Phase 4.

## Current implementation

Phase 1's C++ static core, separate runtime/editor targets, source-only feature-scene skeleton, dependency record, and three-platform CI workflow are in the repository. The new executables currently check a project's `game.lua` entry point; they do not run or edit a 3D game yet. The old 2D SDL3/Lua program is available as the optional `threepoint-2d-prototype` target. Local core and prototype tests pass; Phase 1's Windows, macOS, and Ubuntu CI gate remains unverified until the workflow runs.

On this Ubuntu development machine, `bash scripts/bootstrap-local.sh` builds both targets and runs all available tests. The new CLI check is `build/threepoint --check-project tests/feature_scene`. Future games will be created and run from source files, with no required editor clicks.
