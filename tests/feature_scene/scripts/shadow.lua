local tp = require("threepoint")
return {
  start = function()
    tp.set_camera({eye = {tp.scene_name() == "shadow_shift" and 0.15 or 0, 0, 6},
                   target = {0, 0, 0}, fov = 60})
    local light = tp.component_get(tp.entity_find("shadow light"), "light")
    assert(light.shadow_range >= 5)
    assert(light.shadows == (tp.scene_name() ~= "shadow_off" and
                             tp.scene_name() ~= "shadow_point_off"))
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt) assert(dt >= 0) end,
}
