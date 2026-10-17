return {
  entities = {
    {name = "HDR spheres", mesh = {mesh = "assets/material_spheres.gltf"}},
    {name = "bright sun", light = {type = "directional", direction = {-0.4, -0.6, -1},
                                    color = {1, 1, 1}, intensity = 20}},
  },
  script = "scripts/hdr.lua",
}
