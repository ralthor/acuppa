# Siege Sling

A 3D slingshot game made entirely from Lua, a source scene, level data, and
generated assets. A sling on a grassy hill launches a heavy red ball at a fort
of wood, stone, and glass blocks guarding three round green grumps. The sling
turns left and right as well as up and down. You have four shots per level;
knock out every grump to win and earn one to three stars.

Level 1 is complete: blocks break apart in bursts of debris when hit hard
enough (glass shatters easily, stone takes a solid hit), grumps pop when they
are struck or fall, and each material has its own impact sound. Win and lose
screens show your score, stars, and best result, which is saved between runs
and shown in the top bar. Press Space or Enter on either screen to play again.

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
| Play again (win/lose screen) | Space or Enter | A button |
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
- `scripts/camera_rig.lua` (views and impact shake), `scripts/drag_aim.lua`,
  and `scripts/debris.lua`: small engine-free modules.
- `make_assets.py`: regenerates every file in `assets/` exactly: textured
  glTF blocks (wood planks, stone courses, frosted glass, grass), the sling,
  ball, grumps, trees, rocks, fence, and far hills, the daytime HDRI, and the
  knock, thud, clink, pop, and twang sounds.

Scoring: wood 100, stone 200, glass 50, each grump 500, and 1000 per unused
shot on a win. Stars come at 3000, 5000, and 6500 points; a win always earns
at least one.

SSAO is off: on this open meadow it drew visible bands across the grass.

## Tests

```sh
ctest --test-dir build -R siege_sling --output-on-failure
```

`siege_sling_rules` and `siege_sling_play_modules` test the pure modules.
`siege_sling_gameplay` runs the real game headlessly with injected keys and
mouse drags. `siege_sling_capture` checks a Vulkan capture of the aim and
follow views. `siege_sling_live_startup` opens the interactive window. `siege_sling_export`
and `siege_sling_export_capture` check the exported player.
