-- Static yard for level 1. The fort, grumps, ball, and arc dots are spawned
-- by scripts/play.lua from levels/level1.lua so their physics come from the
-- rules' material table.
return {
  entities = {
    {name = "grass", mesh = {mesh = "assets/grass.gltf"},
     transform = {position = {0, 0, -15}, scale = {150, 1, 150}}},
    {name = "ground", transform = {position = {0, -0.5, -15}},
     physics = {motion = "static", shape = "box", half_extents = {150, 0.5, 150},
                friction = 0.8}},
    {name = "hill", mesh = {mesh = "assets/hill.gltf", casts_shadows = false}, transform = {position = {0, 0, 0}}},
    {name = "hill top", transform = {position = {0, 1, 0}},
     physics = {motion = "static", shape = "box", half_extents = {2.4, 1, 2.4}, friction = 0.8}},
    {name = "sling stem", transform = {position = {0, 2.45, 0.15}, scale = {0.12, 0.45, 0.12}},
     mesh = {mesh = "assets/sling_wood.gltf"}},
    {name = "sling arm left", transform = {position = {-0.27, 3.15, 0.15}, rotation = {0, 0, 0.42},
                                           scale = {0.08, 0.36, 0.08}},
     mesh = {mesh = "assets/sling_wood.gltf"}},
    {name = "sling arm right", transform = {position = {0.27, 3.15, 0.15}, rotation = {0, 0, -0.42},
                                            scale = {0.08, 0.36, 0.08}},
     mesh = {mesh = "assets/sling_wood.gltf"}},
    {name = "sling band", transform = {position = {0, 3.42, 0.15}, scale = {0.46, 0.035, 0.035}},
     mesh = {mesh = "assets/band.gltf"}},
    {name = "sun", transform = {position = {0, 0, -11}},
     light = {type = "directional", direction = {-0.55, -1, -0.45},
              color = {1, 0.95, 0.85}, intensity = 2.6, shadows = true, shadow_range = 16}},
    {name = "view", camera = {eye = {0, 5.9, 7.6}, target = {0, 1.5, -18}, fov = 45,
                              far = 200, active = true}},
  },
  script = "scripts/play.lua",
}
