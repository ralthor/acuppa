local tp = require("acuppa")
local visits

return {
  start = function()
    local state = tp.load("menu_progress") or {visits = 0}
    visits = state.visits + 1
    tp.save("menu_progress", {visits = visits})
    tp.spawn({mesh = "builtin:cube", position = {visits, 0, 0}})
  end,
  fixed_update = function() end,
  update = function() end,
  draw = function()
    local w = tp.viewport_size()
    tp.ui_rect(0, 0, w, 90, 0.04, 0.07, 0.12, 0.9)
    tp.ui_text(36, 30, "VISITS " .. visits, 2)
  end,
}
