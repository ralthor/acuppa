return {
  entities = {
    {name = "ground", transform = {position = {0, -0.25, 0}, scale = {8, 0.5, 8}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "static", shape = "box", half_extents = {4, 0.25, 4}}},
    {name = "falling", transform = {position = {0, 4, 0}, scale = {0.8, 0.8, 0.8}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "dynamic", shape = "box", half_extents = {0.4, 0.4, 0.4}}},
    {name = "trigger", transform = {position = {0, 2, 0}},
     physics = {motion = "static", shape = "box", half_extents = {1, 0.2, 1},
                trigger = true}},
    {name = "view", camera = {eye = {0, 2.5, 9}, target = {0, 1.5, 0}, active = true}},
  },
  script = "scripts/physics.lua",
}
