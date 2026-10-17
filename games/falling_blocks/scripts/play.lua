local tp = require("threepoint")
local Board = require("scripts.board")
local board, rendered, borders, previous, hold_time, gravity, score, lines

local function block(x, y, z, sx, sy)
  return tp.spawn({mesh = "builtin:cube", position = {x, y, z},
                   scale = {sx or 0.46, sy or 0.46, 0.38}})
end

local function draw_board()
  for _, id in ipairs(rendered) do tp.entity_destroy(id) end
  rendered = {}
  for y = 0, Board.height - 1 do
    for x = 0, Board.width - 1 do
      if board.rows[y + 1][x + 1] then
        rendered[#rendered + 1] = block(x - 4.5, y - 9.5, 0)
      end
    end
  end
  if board.active then
    for _, cell in ipairs(board:cells()) do
      rendered[#rendered + 1] = block(cell[1] - 4.5, cell[2] - 9.5, 0.25)
    end
  end
end

local function start_game()
  board = Board.new()
  board:spawn()
  gravity, score, lines = 0, 0, 0
  rendered = rendered or {}
  draw_board()
end

local function award(cleared, drop_distance)
  score = score + 2 * drop_distance + Board.line_score(cleared, 1)
  lines = lines + cleared
  board:spawn()
end

local function pressed(name)
  local down = tp.action(name) > 0
  local fresh = down and not previous[name]
  previous[name] = down
  return fresh, down
end

local function move_held(name, direction, dt)
  local fresh, down = pressed(name)
  if not down then hold_time[name] = 0; return false end
  if fresh then
    hold_time[name] = 0
    return board:move(direction, 0)
  end
  local old = hold_time[name]
  hold_time[name] = old + dt
  if hold_time[name] >= 0.22 and
     math.floor((hold_time[name] - 0.22) / 0.08) >
     math.floor((old - 0.22) / 0.08) then
    return board:move(direction, 0)
  end
  return false
end

return {
  start = function()
    tp.set_camera({eye = {0, 0, 24}, target = {0, 0, 0}, fov = 60})
    borders, rendered, previous, hold_time = {}, {}, {}, {}
    for y = 0, Board.height - 1 do
      borders[#borders + 1] = block(-5.52, y - 9.5, -0.2, 0.08, 0.5)
      borders[#borders + 1] = block(5.52, y - 9.5, -0.2, 0.08, 0.5)
    end
    for x = 0, Board.width - 1 do
      borders[#borders + 1] = block(x - 4.5, -10.52, -0.2, 0.5, 0.08)
    end
    start_game()
  end,
  fixed_update = function(dt)
    if board.game_over then return end
    local interval = tp.action("soft_drop") > 0 and 0.07 or 0.7
    gravity = gravity + dt
    if gravity >= interval then
      gravity = gravity - interval
      local moved, cleared = board:fall()
      if not moved then award(cleared, 0) end
      draw_board()
    end
  end,
  update = function(dt)
    if pressed("restart") then start_game() end
    if board.game_over then return end
    local changed = move_held("left", -1, dt)
    changed = move_held("right", 1, dt) or changed
    if pressed("rotate") then changed = board:rotate() or changed end
    if pressed("hard_drop") then
      local distance, cleared = board:hard_drop()
      award(cleared, distance)
      gravity = 0
      changed = true
    end
    if changed then draw_board() end
  end,
  draw = function() assert(board and rendered) end,
}
