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
    threepoint.set_rotation(cube, {0.3, threepoint.time() * 0.9, 0})
  end,
  draw = function()
    assert(fixed_ticks > 0)
  end,
}
