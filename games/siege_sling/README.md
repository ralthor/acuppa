# Siege Sling

A 3D slingshot game made entirely from Lua, a source scene, level data, and
generated assets. A sling on a grassy hill launches a heavy red ball at a fort
of wood, stone, and glass blocks guarding three round green grumps. The sling
turns left and right as well as up and down. You have four shots per level;
knock out every grump to win and earn one to three stars.

*In development:* level 1 is playable. Broken blocks and defeated grumps are
counted and scored but not yet removed, and the win/lose screens are minimal
(Phase 32). Textures arrive in Phase 33.

From the repository root, run:

```sh
build/acuppa run games/siege_sling
```

| Action | Mouse / keyboard | Controller |
| --- | --- | --- |
| Pull and fire | Left-drag back from anywhere, release to fire | — |
| Turn aim left / right | A / D or Left / Right | Left stick X or D-pad |
| Raise / lower aim | W / S or Up / Down | Left stick Y or D-pad |
| Less / more power | Q / E | Left / right trigger |
| Fire | Space | A button |
| Orbit the aim camera | Right-drag | Right stick |
| Restart | R | Back |
| Exit | Escape | Start |

Dragging works like a real sling: pull down to lob higher, pull sideways to
aim the other way, and pull farther for more power. The dotted arc previews
the flight while you aim or drag; a tiny drag cancels instead of firing.

The camera waits behind the sling, follows the ball in flight, holds a view of
the fort while blocks settle, then eases back to the sling for the next shot.

## Files

- `game.lua`: window, first scene, and input actions.
- `scenes/level1.scene.lua`: the static yard (ground, hill, sling, sun, camera).
- `levels/level1.lua`: shots, star scores, and the fort's blocks and grumps.
  `scripts/play.lua` spawns them with physics from the rules' material table.
- `scripts/rules.lua`: pure aim, launch, arc, damage, shot-flow, and score rules.
- `scripts/camera_rig.lua`, `scripts/drag_aim.lua`: small engine-free modules.
- `make_assets.py`: regenerates every file in `assets/` exactly; the meshes
  carry UVs ready for textures.

SSAO is off: on this open meadow it drew visible bands across the grass.

## Tests

```sh
ctest --test-dir build -R siege_sling --output-on-failure
```

`siege_sling_rules` and `siege_sling_play_modules` test the pure modules.
`siege_sling_gameplay` runs the real game headlessly with injected keys and
mouse drags. `siege_sling_capture` checks a Vulkan capture of the aim and
follow views. `siege_sling_live_startup` opens the interactive window.
