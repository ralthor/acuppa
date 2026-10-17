local threepoint = require("threepoint")
local cube
local fixed_ticks = 0

return {
  start = function()
    cube = threepoint.spawn({mesh = "builtin:cube", rotation = {0.3, 0, 0}})
  end,
  fixed_update = function(dt)
    assert(dt > 0)
    fixed_ticks = fixed_ticks + 1
  end,
  update = function(dt)
    assert(dt >= 0)
    local turn = threepoint.action("turn")
    threepoint.set_rotation(cube, {0.3, threepoint.time() * 0.9 + turn * 0.9, 0})
    local eye_x = threepoint.action("camera_right") + threepoint.action("pan")
    local eye_z = 4.5 + threepoint.action("select") + threepoint.action("jump")
    threepoint.set_camera({eye = {eye_x, 0, eye_z}})
    local x, y = threepoint.pointer()
    local ray = threepoint.screen_ray(x, y)
    assert(ray.direction[3] < 0)
  end,
  draw = function()
    assert(fixed_ticks > 0)
  end,
}
