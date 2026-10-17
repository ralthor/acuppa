# 3D feature-scene coverage

This is the source-only project used for integration checks. It runs in the 3D runtime with no editor action. Each feature phase must add a scene/script case and an automated assertion before marking its row covered. Renderer rows also need captured-frame checks; editor and export rows need their own tests.

| Capability | Phase | Scene/script case | Automated check | Status |
| --- | --- | --- | --- | --- |
| Project source layout | 1 | `game.lua`, `scenes/main.scene.lua`, `scripts/main.lua` | `core_project_check`, CLI checks | Covered |
| Vulkan window, clear, resize, missing driver | 2 | N/A: renderer bootstrap precedes Lua scene loading | `vulkan_smoke_test.py` GPU capture and live resize | Local covered; hosted CI pending |
| Transforms, camera, depth, cube | 3 | Built-in cube preview and Phase 4 Lua cube | `camera_math`, `cube_capture` GPU angle/depth/resize checks | Local covered; hosted CI pending |
| Lua 3D callbacks and source scene | 4 | `game.lua`, `scenes/main.scene.lua`, `scripts/main.lua`: spawn, rotate, time, fixed/update/draw | `lua_scene_logic`, `lua_scene_capture`; invalid API and path cases | Local covered; hosted CI pending |
| Keyboard, mouse, controller, screen ray | 5 | `game.lua` named actions; `scripts/main.lua` turns cube, moves camera, reads pointer ray | `input_actions_and_ray`, `lua_scene_logic` injected events, `input_scene_capture` GPU comparison | Local covered; physical controller and hosted CI pending |
| Entity IDs, components, lights | 6 | Pending | Pending | Pending |
| Prefabs, spawn/destroy, scene switching | 7 | Pending | Pending | Pending |
| glTF meshes and scene hierarchy | 8 | Pending | Pending | Pending |
| Textures, materials, direct-light PBR | 9 | Pending | Pending | Pending |
| HDR target, exposure, tonemapping | 10 | Pending | Pending | Pending |
| HDRI and image-based lighting | 11 | Pending | Pending | Pending |
| Soft shadows | 12 | Pending | Pending | Pending |
| SSAO | 13 | Pending | Pending | Pending |
| 3D bodies, colliders, triggers | 14 | Pending | Pending | Pending |
| Lua physics forces, queries, contacts | 15 | Pending | Pending | Pending |
| Spatial audio and Lua control | 16 | Pending | Pending | Pending |
| Script UI, timers, save/load | 17 | Pending | Pending | Pending |
| Human editor tree and inspector | 18 | Pending | Pending | Pending |
| Viewport selection and gizmos | 19 | Pending | Pending | Pending |
| Source round trip and agent control | 20 | Pending | Pending | Pending |
| Headless tests and exported game | 21 | Pending | Pending | Pending |
| Both code-only sample games | 22–23 | Pending | Pending | Pending |

The public Lua API checklist and sample-game requirements are in [PLAN.md](../../PLAN.md). The final coverage audit checks that each public call has a runnable example, an invalid-input test, and a feature-scene case.
