-- Source-authored gradient sky behind lit spheres and unlit cubes (Phase 29).
-- Aliases: sky (gradient and sun), sky_off (no sky), sky_clear, sky_switch.
return {
  entities = {
    {name = "material spheres", mesh = {mesh = "assets/material_spheres.gltf"},
     transform = {position = {0, 1.2, 0}, scale = {0.8, 0.8, 0.8}}},
    {name = "left block", mesh = {mesh = "builtin:cube"},
     transform = {position = {-3.2, 0.4, -1}, rotation = {0, 0.5, 0}, scale = {0.8, 0.8, 0.8}}},
    {name = "right block", mesh = {mesh = "builtin:cube"},
     transform = {position = {3.2, 0.4, -1}, rotation = {0, -0.5, 0}, scale = {0.8, 0.8, 0.8}}},
    {name = "sun light", light = {type = "directional", direction = {-0.35, -0.7, -0.6},
                                   color = {1, 0.96, 0.88}, intensity = 4}},
    {name = "view", camera = {eye = {0, 1.2, 7}, target = {0, 1.8, 0}, active = true}},
  },
  script = "scripts/sky.lua",
}
