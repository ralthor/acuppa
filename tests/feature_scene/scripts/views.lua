local tp = require("acuppa")

-- A second camera looks at a cube far to the side and is drawn into the
-- top-right corner; the main camera sees a red wall behind blue glass.
local inset = {eye = {20, 0, 6}, target = {20, 0, 0}, fov = 50, near = 0.1, far = 50,
               x = 980, y = 40, width = 260, height = 180}

return {
  start = function()
    assert(not pcall(tp.set_inset))
    assert(not pcall(tp.set_inset, "corner"))
    assert(not pcall(tp.set_inset, {width = 8, height = 100}))
    assert(not pcall(tp.set_inset, {width = 100, height = 5000}))
    assert(not pcall(tp.set_inset, {x = 1.5, width = 100, height = 100}))
    assert(not pcall(tp.set_inset, {fov = 0, width = 100, height = 100}))
    assert(not pcall(tp.set_inset, {eye = {0, 0, 0}, target = {0, 0, 0}, width = 100,
                                    height = 100}))
    tp.set_inset(inset)
    tp.set_inset({x = 900})  -- Omitted fields keep the current inset's values.
    tp.set_inset({x = 980})
    if tp.scene_name() == "views_off" then tp.set_inset(nil) end
  end,
  fixed_update = function() end,
  update = function() end,
}
