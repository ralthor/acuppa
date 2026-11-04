-- See-through glTF materials and a second inset camera (Phase 39).
-- Aliases: views (inset in the top-right corner) and views_off (no inset).
return {
  entities = {
    {name = "red wall", mesh = {mesh = "assets/red_quad.gltf"},
     transform = {scale = {3, 3, 3}}},
    {name = "blue glass", mesh = {mesh = "assets/blend_quad.gltf"},
     transform = {position = {-1.5, 0, 1}, scale = {1.5, 1.5, 1.5}}},
    {name = "inset subject", mesh = {mesh = "builtin:cube"},
     transform = {position = {20, 0, 0}, scale = {1.2, 1.2, 1.2}}},
    {name = "view", camera = {eye = {0, 0, 6}, target = {0, 0, 0}, fov = 60, active = true}},
  },
  script = "scripts/views.lua",
}
