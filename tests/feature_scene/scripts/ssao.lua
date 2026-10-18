local tp = require("acuppa")
return {
  start = function()
    local scene = tp.scene_name()
    tp.set_camera({eye = {scene == "ssao_shift" and 0.15 or 0, 0, 6},
                   target = {0, 0, 0}, fov = 60})
    tp.set_ssao(scene == "ssao_off" and 0 or (scene == "ssao_low" and 1 or 3),
                0.35, 2.0)
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt) assert(dt >= 0) end,
}
