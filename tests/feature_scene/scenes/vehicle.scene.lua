return {
  entities = {
    {name = "floor", transform = {position = {0, -0.5, 0}, scale = {12, 0.5, 12}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "static", shape = "box", half_extents = {12, 0.5, 12}}},
    {name = "car", transform = {position = {0, 0.75, 0}, scale = {0.65, 0.2, 1.05}},
     mesh = {mesh = "builtin:cube"},
     physics = {motion = "dynamic", shape = "box", half_extents = {0.65, 0.2, 1.05}}},
    {name = "view", camera = {eye = {0, 6, 12}, target = {0, 0, -4}, active = true}},
  },
  script = "scripts/vehicle.lua",
}
