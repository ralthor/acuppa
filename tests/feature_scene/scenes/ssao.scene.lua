return {
  entities = {
    {name = "AO receiver", mesh = {mesh = "assets/shadow_plane.gltf"},
     transform = {scale = {2.5, 2.5, 1}}},
    {name = "contact block", mesh = {mesh = "builtin:cube"},
     transform = {position = {0, 0, 0.10}, scale = {0.25, 0.25, 0.10}}},
    {name = "AO light", light = {type = "directional", direction = {0.2, -0.3, -1},
                                  color = {1, 1, 1}, intensity = 3}},
  },
  script = "scripts/ssao.lua",
}
