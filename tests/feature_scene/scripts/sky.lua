local tp = require("acuppa")
local ticks = 0

-- Bright, stylized day sky for the slingshot game.
local day = {
  zenith = {0.04, 0.2, 1.1}, horizon = {0.8, 0.9, 1.05}, ground = {0.2, 0.3, 0.12},
  sun_direction = {0.45, 0.62, -1.6}, sun_color = {14, 12, 9}, sun_size = 2.2,
}

return {
  start = function()
    tp.set_environment("assets/test_gradient.hdr", 0.5)  -- Lighting stays HDRI-based.
    if tp.scene_name() == "sky_off" then return end
    tp.set_sky({zenith = {1, 0, 0}})      -- Partial table: other fields use defaults.
    tp.set_sky(nil)
    tp.set_sky(day)
    -- Each invalid call raises and leaves the day sky in place.
    for _, bad in ipairs({
      {zenith = {1, 0}}, {zenith = {1, 0, -0.1}}, {horizon = {0, 65, 0}},
      {ground = {0 / 0, 0, 0}}, {sun_color = {math.huge, 1, 1}}, {zenith = "blue"},
      {zenith = {"1", 0, 0}}, {sun_direction = {0, 0, 0}}, {sun_direction = {0 / 0, 1, 0}},
      {sun_size = -1}, {sun_size = 21}, {sun_size = "big"}, {sun_size = 0 / 0},
      {clouds = true}, {[1] = 2},
    }) do
      local ok, message = pcall(tp.set_sky, bad)
      assert(not ok and tostring(message):find("sky"), tostring(message))
    end
    assert(not pcall(tp.set_sky))
    assert(not pcall(tp.set_sky, day, day))
    assert(not pcall(tp.set_sky, "day"))
    assert(not pcall(tp.set_sky, 5))
  end,
  fixed_update = function()
    ticks = ticks + 1
    if ticks == 1 and tp.scene_name() == "sky_clear" then tp.set_sky(nil) end
    if ticks == 1 and tp.scene_name() == "sky_switch" then tp.scene_switch("sky_off") end
  end,
  update = function() end,
}
