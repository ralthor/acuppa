-- Rockets fly in full 3D over a 3 m square pad at the origin
-- (see neat_rocket/runner/rocket.hpp). scripts/viewer.lua sets the camera.
return {
  entities = {
    {name = "ground", transform = {position = {0, -0.5, 0}, scale = {45, 0.5, 45}},
     mesh = {mesh = "assets/ground.gltf"}},
    {name = "pad", transform = {position = {0, 0.01, 0}, scale = {1.5, 0.02, 1.5}},
     mesh = {mesh = "assets/pad.gltf", casts_shadows = false}},
    {name = "sun", transform = {position = {0, 0, 0}},
     light = {type = "directional", direction = {-0.35, -1, -0.45},
              color = {0.85, 0.9, 1}, intensity = 2.2, shadows = true, shadow_range = 24}},
  },
  script = "scripts/viewer.lua",
}
