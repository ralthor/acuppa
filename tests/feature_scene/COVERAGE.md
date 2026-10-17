# 3D feature-scene coverage

This is the source-only project used for integration checks. It runs in the 3D runtime with no editor action. Each feature phase must add a scene/script case and an automated assertion before marking its row covered. Renderer rows also need captured-frame checks; editor and export rows need their own tests.

| Capability | Phase | Scene/script case | Automated check | Status |
| --- | --- | --- | --- | --- |
| Project source layout | 1 | `game.lua`, `scenes/main.scene.lua`, `scripts/main.lua` | `core_project_check`, CLI checks | Covered |
| Vulkan window, clear, resize, missing driver | 2 | N/A: renderer bootstrap precedes Lua scene loading | `vulkan_smoke_test.py` GPU capture and live resize | Local covered; hosted CI pending |
| Transforms, camera, depth, cube | 3 | Built-in cube preview and Phase 4 Lua cube | `camera_math`, `cube_capture` GPU angle/depth/resize checks | Local covered; hosted CI pending |
| Lua 3D callbacks and source scene | 4 | `game.lua`, `scenes/main.scene.lua`, `scripts/main.lua`: spawn, rotate, time, fixed/update/draw | `lua_scene_logic` checks drawing before the first fixed tick; `lua_scene_capture` and `lua_live_startup` check captured and interactive Vulkan paths; invalid API and path cases | Local covered; hosted CI pending |
| Keyboard, mouse, controller, screen ray | 5 | `game.lua` named actions; `scripts/main.lua` turns cube, moves camera, reads pointer ray | `input_actions_and_ray`, `lua_scene_logic` injected events, `input_scene_capture` GPU comparison | Local covered; physical controller and hosted CI pending |
| Entity IDs, transform parenting, camera, light, mesh, script components | 6 | `scripts/main.lua` creates, changes, reads, removes every component; camera and ray use active component | `world_components`, `lua_scene_logic`, `lua_component_errors`; prior GPU captures | Local covered; lighting and per-entity script execution arrive in later phases |
| Source scenes, named parents, prefabs, per-entity scripts, reload/switch | 7 | `scenes/alternate.scene.lua`, `prefabs/probe.prefab.lua`, `scripts/actor.lua`, `scripts/alternate.lua` | `source_scene_and_prefabs` checks callbacks, override, rollback, reload, switch, invalid source | Local covered; hosted CI pending |
| glTF meshes and scene hierarchy | 8 | `assets/triangle.gltf`, external-buffer and GLB variants; `scenes/imported.scene.lua`, `scripts/imported.lua` | `gltf_geometry_and_errors` validates hierarchy, formats, failures; `gltf_scene_capture` verifies visible Vulkan pixels | Local covered for static triangles; hosted CI pending |
| Textures, materials, direct-light PBR | 9 | `assets/material_quad.gltf`, `assets/material_spheres.gltf`, `scenes/textured.scene.lua`, `scenes/spheres.scene.lua`, `scenes/point.scene.lua`, `scripts/materials.lua` cover textures, factors, directional/point lights, and Lua light updates | `gltf_geometry_and_errors` covers materials and invalid images/samplers; `pbr_material_capture` checks textured pixels, lit/dark PBR spheres, and point light under Vulkan validation | Local covered; hosted CI pending |
| HDR target, exposure, tonemapping | 13 | Pending | Pending | Pending |
| HDRI and image-based lighting | 14 | Pending | Pending | Pending |
| Soft shadows | 15 | Pending | Pending | Pending |
| SSAO | 16 | Pending | Pending | Pending |
| 3D bodies, colliders, triggers | 17 | Pending | Pending | Pending |
| Lua physics forces, queries, contacts | 18 | Pending | Pending | Pending |
| Script UI, timers, save/load | 19 | Pending | Pending | Pending |
| Human editor tree and inspector | 20 | Pending | Pending | Pending |
| Viewport selection and gizmos | 21 | Pending | Pending | Pending |
| Source round trip and agent control | 22 | Pending | Pending | Pending |
| Headless tests and exported game | 23 | Pending | Pending | Pending |
| Spatial audio and Lua control | 25 | Pending | Pending | Pending |
| Both code-only sample games | 10–12, 24, 26 | `games/falling_blocks` is a separate source-only Lua game; the feature scene also checks project-local `require` | Rule and full-match tests, live startup, and Vulkan captures cover play and visible feedback; export follows | First game complete; export and second game pending |

The public Lua API checklist and sample-game requirements are in [PLAN.md](../../PLAN.md). The final coverage audit checks that each public call has a runnable example, an invalid-input test, and a feature-scene case.

Phases 10–12 build and test the falling-block game in its own checked-in source project. New engine APIs added for that game must also get cases in this feature scene.
