-- Shared state across scenes: the level list, the chosen level, and saved
-- best results. `require` caches this module for the whole game run, so the
-- menu sets Session.index and the play scene reads it after a scene switch.
-- Levels load with tp.data_read, so a newly written level appears without a
-- restart.
local tp = require("acuppa")
local Rules = require("scripts.rules")

local Session = {index = 1, INDEX = "levels/index.lua", SLOT = "progress"}

-- Ordered {file, name} entries from levels/index.lua, validated.
function Session.levels()
  local entries = tp.data_read(Session.INDEX)
  if type(entries) ~= "table" or #entries == 0 then
    error(Session.INDEX .. " must list at least one level", 2)
  end
  local seen = {}
  for i, entry in ipairs(entries) do
    local where = Session.INDEX .. "[" .. i .. "]"
    if type(entry) ~= "table" or type(entry.file) ~= "string" or
       not entry.file:match("^levels/[%w_%-]+%.lua$") or entry.file == Session.INDEX then
      error(where .. ".file must name a levels/<name>.lua file", 2)
    end
    if type(entry.name) ~= "string" or entry.name == "" or #entry.name > Rules.LIMITS.max_name then
      error(where .. ".name must be a nonempty string of at most 48 bytes", 2)
    end
    if seen[entry.file] then error(where .. " repeats " .. entry.file, 2) end
    seen[entry.file] = true
  end
  return entries
end

-- Loads and checks indexed level i: returns the level data and its entry.
function Session.load(i)
  local entries = Session.levels()
  local entry = entries[i]
  if not entry then error("no indexed level " .. tostring(i), 2) end
  local level = tp.data_read(entry.file)
  if not level then error(entry.file .. " is listed in " .. Session.INDEX .. " but missing", 2) end
  local ok, message = pcall(Rules.check_playable, level)
  if not ok then error(entry.file .. ": " .. tostring(message), 2) end
  return level, entry, #entries
end

-- Loads every indexed level so a broken file fails `acuppa test`.
function Session.check_all()
  local entries = Session.levels()
  for i = 1, #entries do Session.load(i) end
  return #entries
end

-- Saved best {score, stars} per level file.
function Session.progress()
  local saved = tp.load(Session.SLOT)
  if type(saved) ~= "table" or type(saved.levels) ~= "table" then return {levels = {}} end
  return saved
end

function Session.best(file) return Session.progress().levels[file] end

function Session.record(file, score, stars)
  local progress = Session.progress()
  local best = progress.levels[file]
  if not best or score > best.score or (score == best.score and stars > best.stars) then
    best = {score = score, stars = stars}
    progress.levels[file] = best
    tp.save(Session.SLOT, progress)
  end
  return best
end

-- Exported games cannot write project files. The probe uses an invalid
-- (non-.lua) path, so in a source game it fails without writing anything.
local exported
function Session.exported()
  if exported == nil then
    local ok, message = pcall(tp.data_write, "levels/probe.txt", {})
    exported = not ok and tostring(message):find("read-only in exported games", 1, true) ~= nil
  end
  return exported
end

return Session
