local tp = require("threepoint")

return {
  start = function()
    local saved = tp.load("roundtrip")
    local x = 0
    if saved == nil then
      assert(tp.load("absent") == nil)
      local state = {level = 4, score = 12.5, won = false,
                     inventory = {"key", "gem"}, metadata = {note = "hello\0world"}}
      tp.save("roundtrip", state)
      local restored = tp.load("roundtrip")
      assert(restored.level == 4 and restored.inventory[2] == "gem")
      assert(restored.metadata.note == "hello\0world")
      assert(not pcall(tp.save, "../bad", state))
      assert(not pcall(tp.save, "bad", {value = function() end}))
      local cycle = {}; cycle.self = cycle
      assert(not pcall(tp.save, "bad", cycle))
      assert(not pcall(tp.save, "bad", {[true] = 1}))
      assert(not pcall(tp.save, "bad", {value = 0 / 0}))
    elseif saved.level == 4 then
      assert(saved.score == 12.5 and saved.won == false)
      assert(saved.inventory[1] == "key")
      x = 1
      saved.level = 8
      tp.save("roundtrip", saved)
    else
      assert(saved.level == 8)
      x = 2
    end
    tp.spawn({mesh = "builtin:cube", position = {x, 0, 0}})
  end,
  fixed_update = function() end,
  update = function() end,
}
