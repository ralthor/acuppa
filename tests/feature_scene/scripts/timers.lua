local tp = require("acuppa")
local ticks = 0

return {
  start = function()
    tp.timer_start("once", 3 / 60)
    tp.timer_start("repeat_tick", 2 / 60, true)
    tp.timer_start("cancelled", 1)
    assert(tp.timer_cancel("cancelled"))
    assert(not tp.timer_cancel("cancelled"))
    assert(tp.timer_remaining("once") == 3 / 60)
    assert(tp.timer_remaining("cancelled") == nil)
    assert(not pcall(tp.timer_start, "bad name", 1))
    assert(not pcall(tp.timer_start, "bad", 0))
    assert(not pcall(tp.timer_start, "bad", math.huge))
    assert(not pcall(tp.timer_start, "bad", 1, "yes"))
    assert(not pcall(tp.timer_remaining, "../bad"))
    assert(#tp.timer_events() == 0)
  end,
  fixed_update = function()
    ticks = ticks + 1
    local events = tp.timer_events()
    local expected = (ticks % 2 == 0 and 1 or 0) + (ticks == 3 and 1 or 0)
    assert(#events == expected)
    if ticks == 3 then assert(events[1] == "once") end
    if ticks > 3 then assert(tp.timer_remaining("once") == nil) end
    if ticks == 6 then assert(tp.timer_cancel("repeat_tick")) end
  end,
  update = function() end,
}
