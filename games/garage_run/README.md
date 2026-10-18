# Garage Run

A short 3D checkpoint and parking game made entirely from Lua, a source scene,
and project-local assets. No editor action is needed to play or change it.

From the repository root, run:

```sh
build/threepoint run games/garage_run
```

Drive through the two pairs of gold posts in order, then stop inside the green
parking bay. The top bar shows the current goal, elapsed time, and wall hits.
Finishing saves the best time in this project's `saves/` directory.

| Action | Keyboard | Controller |
| --- | --- | --- |
| Accelerate / reverse | W / S | Right / left trigger |
| Steer | A / D | Left stick or D-pad |
| Brake | Space | A button |
| Restart | R | Back |
| Exit | Escape | Start |

The car is a dynamic Jolt box body with Lua arcade steering; walls and the
obstacle are solid bodies, while gates and the parking bay are triggers. The
course uses PBR glTF meshes, an HDRI, a shadowed light, SSAO, spatial engine and
impact sounds, and Lua UI. The small checked-in assets reproduce exactly with
`make_assets.py`; this is only needed when changing the asset generator.

Automated rules, gameplay, asset, Vulkan capture, and interactive startup tests
run with the engine suite. This source project has not yet been checked as a
standalone exported game because the export command remains a later phase.
