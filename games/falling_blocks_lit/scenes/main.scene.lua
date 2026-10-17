return {
  entities = {
    {name = "studio light", light = {type = "directional", direction = {0.45, -0.7, -1},
                                      color = {1, 0.92, 0.82}, intensity = 2.2}},
    {name = "board panel", mesh = {mesh = "assets/panel.gltf"},
     transform = {position = {0, 0, -0.45}, scale = {5.42, 10.45, 1}}},
  },
  script = "scripts/play.lua",
}
