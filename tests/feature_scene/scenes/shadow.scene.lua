local tp = require("threepoint")
local scene = tp.scene_name()
local point = scene == "shadow_point" or scene == "shadow_point_off" or
              scene == "shadow_point_second"
local entities = {
    {name = "receiver", mesh = {mesh = "assets/shadow_plane.gltf"},
     transform = {scale = {2.5, 2.5, 1}}},
    {name = "occluder", mesh = {mesh = "builtin:cube"},
     transform = {position = {0, 0, 0.6}, scale = {0.25, 0.25, 0.25}}},
    {name = "masked-out caster", mesh = {mesh = "assets/shadow_cutout.gltf"},
     transform = {position = {-1, 1, 0.6}, scale = {0.25, 0.25, 1}}},
    {name = "shadow light", transform = {position = point and {1.2, 1, 2.5} or {0, 0, 0}},
     light = {type = point and "point" or "directional",
              direction = {1, -0.5, -1}, color = {1, 1, 1},
              intensity = point and 18 or 4,
              shadows = scene ~= "shadow_off" and scene ~= "shadow_point_off",
              shadow_range = point and 8 or 5}},
}
if scene == "shadow_point_second" then
  table.insert(entities, 4, {name = "unshadowed first light",
                            light = {type = "directional", intensity = 0}})
end
return {
  entities = entities,
  script = "scripts/shadow.lua",
}
