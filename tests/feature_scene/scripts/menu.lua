local tp = require("acuppa")
local help = false

return {
  start = function()
    assert(not pcall(tp.ui_text, 10, 10, "outside draw", 1))
  end,
  fixed_update = function() end,
  update = function() end,
  draw = function()
    local w, h = tp.viewport_size()
    local center = (w - 300) / 2
    assert(not pcall(tp.viewport_size, 1))
    tp.ui_rect(0, 0, w, h, 0.04, 0.07, 0.12, 1)
    tp.ui_text(center - 20, h / 6, "ACUPPA", 4)
    if tp.ui_button("start", center, h / 2 - 100, 300, 64, "START GAME") then
      tp.scene_switch("menu_play")
    end
    assert(not pcall(tp.ui_button, "start", 0, 0, 10, 10, "DUPLICATE"))
    if tp.ui_button("help", center, h / 2 - 10, 300, 64, "HELP") then
      help = not help
    end
    local focused = tp.ui_focus()
    assert(focused == "start" or focused == "help")
    assert(tp.ui_focus(focused) == focused)
    assert(not pcall(tp.ui_focus, ""))
    if help then tp.ui_text(center - 15, h / 2 + 110, "TAB: FOCUS  ENTER: SELECT", 2) end
    assert(not pcall(tp.ui_rect, 0, 0, -1, 10, 1, 1, 1, 1))
    assert(not pcall(tp.ui_text, 0, 0, "bad\ntext", 1))
  end,
}
