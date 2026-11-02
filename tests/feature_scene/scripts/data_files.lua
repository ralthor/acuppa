local tp = require("acuppa")

-- Project data files: write, read back, overwrite, and reject invalid paths
-- or values. Generated files go under levels/generated/ in the test copy.
local out = "levels/generated/written.lua"

local function rejects(call, ...)
  local ok, err = pcall(call, ...)
  assert(not ok, "invalid data call succeeded")
  return err
end

return {
  start = function()
    local authored = tp.data_read("levels/authored.lua")
    assert(authored.name == "authored" and authored.blocks[2].kind == "stone")
    assert(authored.blocks[2].position[1] == 1.25)
    assert(tp.data_read("levels/missing.lua") == nil)
    assert(tp.data_read("game.lua").first_scene == "main")

    local previous = tp.data_read(out)
    local runs = previous and previous.runs + 1 or 1
    local level = {runs = runs, name = "written \"quoted\"\n", gravity = -9.81,
                   tiny = 1e-300, big = 2 ^ 53, neg = -42, pos = {1.5, -2.25, 3},
                   flags = {true, false},
                   blocks = {{kind = "wood", x = 0.1}, {kind = "stone", x = -0.3}},
                   ["odd key"] = 1, [10] = "sparse"}
    tp.data_write(out, level)
    local back = tp.data_read(out)
    assert(back.runs == runs and back.name == level.name and back.gravity == -9.81)
    assert(back.tiny == 1e-300 and back.big == 2 ^ 53 and math.type(back.big) == "float")
    assert(math.type(back.neg) == "integer" and back.pos[2] == -2.25 and back.flags[2] == false)
    assert(back.blocks[2].kind == "stone" and back["odd key"] == 1 and back[10] == "sparse")
    back.extra = "overwritten"
    tp.data_write(out, back)
    assert(tp.data_read(out).extra == "overwritten")

    rejects(tp.data_write, "/tmp/abs.lua", {})
    rejects(tp.data_write, "levels/../game.lua", {})
    rejects(tp.data_write, "game.lua", {})
    rejects(tp.data_write, "scripts/data_files.lua", {})
    rejects(tp.data_write, "levels/generated/x.txt", {})
    rejects(tp.data_write, out, "text")
    rejects(tp.data_write, out, {f = function() end})
    rejects(tp.data_write, out, {v = 0 / 0})
    local cycle = {}; cycle.self = cycle
    rejects(tp.data_write, out, cycle)
    rejects(tp.data_write, 7, {})
    assert(tp.data_read(out).extra == "overwritten", "failed write changed the file")
    local err = rejects(tp.data_read, "scripts/data_files.lua")
    assert(err:find("scripts/data_files.lua", 1, true), err)
    rejects(tp.data_read, "../outside.lua")
    rejects(tp.data_read, "levels/authored.txt")
    tp.spawn({mesh = "builtin:cube", position = {runs, 0, 0}})
  end,
  fixed_update = function() end,
  update = function() end,
}
