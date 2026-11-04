-- Landers fly in the X/Y plane over a pad at x = 0 (see neat_lab/runner/lander.hpp).
-- Species ghosts are drawn at increasing depth behind the champion, so the
-- pad and ground extend backward to z = -10. scripts/viewer.lua sets the
-- camera right of the pad so the network panel does not cover the flight area.
return {
  entities = {
    {name = "ground", transform = {position = {0, -0.5, -4}, scale = {45, 0.5, 14}},
     mesh = {mesh = "assets/ground.gltf"}},
    {name = "pad", transform = {position = {0, 0.01, -4}, scale = {1.0, 0.02, 6}},
     mesh = {mesh = "assets/pad.gltf", casts_shadows = false}},
    {name = "sun", transform = {position = {0, 0, -4}},
     light = {type = "directional", direction = {-0.35, -1, -0.45},
              color = {0.85, 0.9, 1}, intensity = 2.2, shadows = true, shadow_range = 24}},
  },
  script = "scripts/viewer.lua",
}
