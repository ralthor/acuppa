return {
  entities = {
    {name = "material spheres", mesh = {mesh = "assets/material_spheres.gltf"}},
    {name = "point light", transform = {position = {0, 0, 3}},
     light = {type = "point", color = {1, 1, 1}, intensity = 25}},
  },
  script = "scripts/point.lua",
}
