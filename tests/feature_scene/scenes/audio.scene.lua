return {
  entities = {
    {name = "floor", transform = {position = {0, -0.25, 0}, scale = {6, 0.5, 6}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "static", shape = "box", half_extents = {3, 0.25, 3}}},
    {name = "ball", transform = {position = {-1, 3, 0}, scale = {0.8, 0.8, 0.8}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "dynamic", shape = "sphere", radius = 0.4}},
    {name = "view", camera = {eye = {0, 2.5, 8}, target = {0, 1, 0}, active = true}},
  },
  script = "scripts/audio.lua",
}
