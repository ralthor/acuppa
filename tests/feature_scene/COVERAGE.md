# 3D feature-scene coverage

This is the source-only project used for integration checks. In Phase 1 only its file layout is checked; it cannot run in the new 3D runtime yet. Each feature phase must add a scene/script case and an automated assertion before marking its row covered. Renderer rows also need captured-frame checks; editor and export rows need their own tests.

| Capability | Phase | Scene/script case | Automated check | Status |
| --- | --- | --- | --- | --- |
| Project source layout | 1 | `game.lua`, `scenes/main.scene.lua`, `scripts/main.lua` | `core_project_check`, CLI checks | Covered |
| Vulkan window, clear, resize, missing driver | 2 | N/A: renderer bootstrap precedes Lua scene loading | `vulkan_smoke_test.py` GPU capture and live resize | Local covered; hosted CI pending |
| Transforms, camera, depth, cube | 3 | Pending | Pending | Pending |
| Lua 3D callbacks and source scene | 4 | Pending | Pending | Pending |
| Keyboard, mouse, controller, screen ray | 5 | Pending | Pending | Pending |
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
