-- Title menu (Play, Levels, Editor, Quit) and level select with the best
-- stars per level. Mouse clicks, Tab/Up/Down focus, and Enter all work.
local tp = require("acuppa")
local Session = require("scripts.session")

local screen = "title"   -- "title" or "levels"
local entries = {}
local note = ""
local held = {}
local order = {}         -- button ids drawn this frame, top to bottom
local activate = false   -- Space pressed: activate the focused button this frame

local function edge(name)
  local down = tp.action(name) > 0
  local pressed = down and not held[name]
  held[name] = down
  return pressed
end

local function play(index)
  Session.index = index
  tp.scene_switch("play")
end

-- First level without a win yet, or level 1 when every level is won.
local function next_unplayed()
  local progress = Session.progress()
  for i, entry in ipairs(entries) do
    if not progress.levels[entry.file] then return i end
  end
  return 1
end

local function button(id, x, y, width, height, label)
  order[#order + 1] = id
  local focused = tp.ui_focus() == id
  if focused then tp.ui_rect(x - 6, y - 6, width + 12, height + 12, 1, 0.8, 0.2, 0.9) end
  local hit = tp.ui_button(id, x, y, width, height, label)
  if not hit and activate and tp.ui_focus() == id then
    activate, hit = false, true
  end
  return hit
end

local function stars(x, y, count)
  for i = 1, 3 do
    local lit = i <= count
    tp.ui_rect(x + (i - 1) * 34, y, 28, 28, lit and 1 or 0.3, lit and 0.78 or 0.33,
               lit and 0.12 or 0.38, 0.95)
    tp.ui_text(x + (i - 1) * 34 + 9, y + 7, "*", 2)
  end
end

local function title(w)
  tp.ui_rect(w / 2 - 290, 56, 580, 116, 0.04, 0.1, 0.22, 0.35)
  tp.ui_text(w / 2 - 264, 72, "SIEGE SLING", 8)
  tp.ui_text(w / 2 - 192, 144, "TOPPLE THE FORT. POP THE GRUMPS.", 2)
end

local function title_screen(w, h)
  local x, y = w / 2 - 150, h / 2 - 90
  tp.ui_rect(x - 30, y - 30, 360, Session.exported() and 290 or 360, 0.04, 0.1, 0.22, 0.72)
  if button("play", x, y, 300, 56, "PLAY") then play(next_unplayed()) end
  y = y + 70
  if button("levels", x, y, 300, 56, "LEVELS") then
    screen, note = "levels", ""
    tp.ui_focus("level_1")
  end
  y = y + 70
  if not Session.exported() then
    if button("editor", x, y, 300, 56, "EDITOR") then
      note = "THE LEVEL EDITOR ARRIVES IN THE NEXT UPDATE"
    end
    y = y + 70
  end
  if button("quit", x, y, 300, 56, "QUIT") then tp.quit() end
  if note ~= "" then tp.ui_text(x - 20, y + 80, note, 1) end
end

local function levels_screen(w, h)
  local progress = Session.progress()
  local rows = #entries
  local x, y = w / 2 - 260, h / 2 - 40 - rows * 33
  tp.ui_rect(x - 30, y - 60, 580, rows * 66 + 150, 0.04, 0.1, 0.22, 0.72)
  tp.ui_text(x, y - 42, "CHOOSE A LEVEL", 3)
  for i, entry in ipairs(entries) do
    local best = progress.levels[entry.file]
    if button("level_" .. i, x, y, 320, 52, string.format("%d  %s", i, entry.name:upper())) then
      play(i)
    end
    stars(x + 340, y + 4, best and best.stars or 0)
    tp.ui_text(x + 446, y + 12, best and string.format("%d", best.score) or "NEW", 2)
    y = y + 66
  end
  if button("back", x, y + 8, 160, 48, "BACK") then
    screen = "title"
    tp.ui_focus("levels")
  end
end

return {
  start = function()
    entries = Session.levels()
    Session.check_all()  -- every indexed level must load and validate
    Session.exported()
    tp.set_exposure(0.95)
    tp.set_environment("assets/day.hdr", 0.7)
    tp.set_sky({zenith = {0.03, 0.17, 1.0}, horizon = {0.42, 0.66, 1.15}, ground = {0.25, 0.5, 0.15},
                sun_direction = {0.55, 1, 0.45}, sun_color = {14, 12, 9}, sun_size = 2})
    tp.set_ssao(0)
    held.exit = tp.action("exit") > 0  -- Escape from a level must not also quit
  end,

  fixed_update = function() end,

  update = function()
    if edge("exit") then
      if screen == "levels" then
        screen = "title"
        tp.ui_focus("levels")
      else
        tp.quit()
      end
    end
    activate = activate or edge("fire")
    local up = edge("aim_up_arrow") or edge("aim_up")
    local down = edge("aim_down_arrow") or edge("aim_down")
    if (up or down) and #order > 0 then
      local current = 1
      for i, id in ipairs(order) do if id == tp.ui_focus() then current = i end end
      current = (current - 1 + (down and 1 or -1)) % #order + 1
      tp.ui_focus(order[current])
    end
  end,

  draw = function()
    local w, h = tp.viewport_size()
    order = {}
    title(w)
    if screen == "title" then title_screen(w, h) else levels_screen(w, h) end
    activate = false
    tp.ui_text(16, h - 22, "MOUSE, OR UP/DOWN AND ENTER/SPACE   ESC BACK", 1)
  end,
}
