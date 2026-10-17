return {
  entities = {
    {name = "textured quad", mesh = {mesh = "assets/material_quad.gltf"}},
    {name = "sun", light = {type = "directional", direction = {0, 0, -1},
                             color = {1, 1, 1}, intensity = 3}},
  },
  script = "scripts/materials.lua",
}
