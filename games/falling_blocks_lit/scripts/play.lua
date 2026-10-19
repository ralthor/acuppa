local tp = require("acuppa")
local Board = require("scripts.board")
local Font = require("scripts.font")
local Feedback = require("scripts.clear_feedback")
local board, rendered, status, message, previous, hold_time
local gravity, score, lines, level, paused
local effect

local function block(x, y, z, sx, sy, sz)
  return tp.spawn({mesh = "builtin:cube", position = {x, y, z},
                   scale = {sx or 0.46, sy or 0.46, sz or 0.38},
                   casts_shadows = false})
end

local function piece_block(kind, x, y, z, sx, sy, sz)
  return tp.spawn({mesh = "assets/" .. kind .. ".gltf", position = {x, y, z},
                   scale = {sx or 0.46, sy or 0.46, sz or 0.38}})
end

local function clear(ids)
  for _, id in ipairs(ids) do tp.entity_destroy(id) end
end

local function write(ids, label, x, y, unit)
  Font.write(label, x, y, 0.8, unit, function(px, py, pz, size)
    ids[#ids + 1] = block(px, py, pz, size, size, 0.08)
  end)
end

local function preview(ids, kind, x, y)
  if not kind then return end
  for _, cell in ipairs(board:cells({kind = kind, x = 0, y = 0, rotation = 0})) do
    ids[#ids + 1] = piece_block(kind, x + cell[1] * 0.58, y + cell[2] * 0.58,
                                0.2, 0.25, 0.25, 0.25)
  end
end

local function draw_status()
  clear(status)
  status = {}
  write(status, "NEXT", 7.4, 8.8, 0.29)
  preview(status, board:peek_kind(), 7.5, 5.9)
  write(status, "SCORE", 7.4, 3.8, 0.29)
  write(status, tostring(score), 7.4, 2.3, 0.31)
  write(status, "LINES", 7.4, 0.4, 0.29)
  write(status, tostring(lines), 7.4, -1.1, 0.31)
  write(status, "LEVEL", 7.4, -2.8, 0.29)
  write(status, tostring(level), 7.4, -4.3, 0.31)
  write(status, "HOLD", -15.0, 8.8, 0.29)
  preview(status, board.held_kind, -15.0, 5.9)
end

local function draw_message()
  clear(message)
  message = {}
  if board.game_over then
    write(message, "GAME OVER", -6.0, 1.8, 0.33)
    write(message, "R RESTART", -5.2, -0.2, 0.27)
  elseif paused then
    write(message, "PAUSED", -4.0, 0.5, 0.33)
  end
end

local function draw_board()
  clear(rendered)
  rendered = {}
  for y = 0, Board.height - 1 do
    for x = 0, Board.width - 1 do
      local kind = board.rows[y + 1][x + 1]
      if kind then
        rendered[#rendered + 1] = piece_block(kind, x - 4.5, y - 9.5, 0)
      end
    end
  end
  if board.active then
    for _, cell in ipairs(board:ghost_cells()) do
      local x, y = cell[1] - 4.5, cell[2] - 9.5
      rendered[#rendered + 1] = block(x - 0.39, y, -0.1, 0.035, 0.42, 0.06)
      rendered[#rendered + 1] = block(x + 0.39, y, -0.1, 0.035, 0.42, 0.06)
      rendered[#rendered + 1] = block(x, y - 0.39, -0.1, 0.42, 0.035, 0.06)
      rendered[#rendered + 1] = block(x, y + 0.39, -0.1, 0.42, 0.035, 0.06)
    end
    for _, cell in ipairs(board:cells()) do
      rendered[#rendered + 1] = piece_block(board.active.kind,
                                             cell[1] - 4.5, cell[2] - 9.5, 0.25)
    end
  end
  draw_message()
end

local function end_effect()
  if not effect then return end
  clear(effect.ids)
  effect = nil
  tp.set_camera({eye = {0, 0, 24}, target = {0, 0, 0}})
end

local function start_effect(rows)
  if #rows == 0 then return end
  end_effect()
  local profile = Feedback.profile(#rows)
  effect = {ids = {}, bars = {}, sparks = {}, elapsed = 0, profile = profile}
  for _, row in ipairs(rows) do
    local y = row - 9.5
    local bar = block(0, y, 0.65, 5.05, 0.23, 0.05)
    effect.ids[#effect.ids + 1] = bar
    effect.bars[#effect.bars + 1] = bar
    for i = 1, profile.sparks do
      local side = i % 2 == 0 and 1 or -1
      local lane = math.floor((i - 1) / 2)
      local x = side * (4.2 - lane * 0.25)
      local spark = block(x, y, 0.72, 0.09, 0.09, 0.06)
      effect.ids[#effect.ids + 1] = spark
      effect.sparks[#effect.sparks + 1] = {id = spark, x = x, y = y,
                                          speed = side * (4 + lane),
                                          rise = (lane - 1) * 3}
    end
  end
  tp.sound_play("assets/clear.wav", {volume = profile.volume, pitch = profile.pitch})
  if #rows >= 3 then
    tp.sound_play("assets/burst.wav", {volume = #rows == 4 and 0.85 or 0.60})
  end
end

local function update_effect(dt)
  if not effect then return end
  effect.elapsed = math.min(effect.profile.duration, effect.elapsed + dt)
  local progress = effect.elapsed / effect.profile.duration
  local fade = 1 - progress
  for _, id in ipairs(effect.bars) do
    tp.component_set(id, "transform", {scale = {5.05 * fade + 0.01,
                                                 0.23 * fade + 0.01, 0.05}})
  end
  for _, spark in ipairs(effect.sparks) do
    tp.component_set(spark.id, "transform", {
      position = {spark.x + spark.speed * effect.elapsed,
                  spark.y + spark.rise * effect.elapsed, 0.72},
      scale = {0.09 * fade + 0.01, 0.09 * fade + 0.01, 0.06},
    })
  end
  local shake = effect.profile.shake * fade * fade
  tp.set_camera({eye = {shake * math.sin(effect.elapsed * 61),
                         shake * math.sin(effect.elapsed * 83), 24},
                 target = {0, 0, 0}})
  if progress >= 1 then end_effect() end
end

local function start_game()
  end_effect()
  board = Board.new(16) -- fixed seed makes game runs and regression captures reproducible
  board:spawn()
  gravity, score, lines, level, paused = 0, 0, 0, 1, false
  hold_time = {}
  draw_status()
  draw_board()
end

local function award(cleared, drop_distance, cleared_rows)
  score = score + 2 * drop_distance + Board.line_score(cleared, level)
  lines = lines + cleared
  level = Board.level_for_lines(lines)
  if cleared > 0 then start_effect(cleared_rows) end
  board:spawn()
  draw_status()
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
    tp.set_exposure(0.85)
    tp.set_environment("assets/studio.hdr", 0.7)
    if tp.scene_name() ~= "ssao_off" then tp.set_ssao(3, 0.65, 0.25) end
    rendered, status, message, previous, hold_time = {}, {}, {}, {}, {}
    local border = {}
    for y = 0, Board.height - 1 do
      border[#border + 1] = block(-5.52, y - 9.5, -0.2, 0.08, 0.5)
      border[#border + 1] = block(5.52, y - 9.5, -0.2, 0.08, 0.5)
    end
    for x = 0, Board.width - 1 do
      border[#border + 1] = block(x - 4.5, -10.52, -0.2, 0.5, 0.08)
    end
    local guide = {}
    write(guide, "FALLING BLOCKS", -10.5, 12.1, 0.36)
    write(guide, "LR MOVE", -16.0, 2.8, 0.21)
    write(guide, "UP ROTATE", -16.0, 1.2, 0.21)
    write(guide, "DOWN SOFT", -16.0, -0.4, 0.21)
    write(guide, "SPACE DROP", -16.0, -2.0, 0.21)
    write(guide, "C HOLD", -16.0, -3.6, 0.21)
    write(guide, "P PAUSE", -16.0, -5.2, 0.21)
    write(guide, "R RESTART", -16.0, -6.8, 0.21)
    start_game()
  end,
  fixed_update = function(dt)
    if board.game_over or paused then return end
    local interval = Board.gravity_interval(level, tp.action("soft_drop") > 0)
    gravity = gravity + dt
    if gravity >= interval then
      gravity = gravity - interval
      local moved, cleared, cleared_rows = board:fall()
      if moved and tp.action("soft_drop") > 0 then
        score = score + 1
        draw_status()
      elseif not moved then
        award(cleared, 0, cleared_rows)
      end
      draw_board()
    end
  end,
  update = function(dt)
    if pressed("restart") then start_game(); return end
    update_effect(dt)
    if pressed("pause") then
      paused = not paused
      draw_message()
    end
    if board.game_over or paused then return end
    local changed = move_held("left", -1, dt)
    changed = move_held("right", 1, dt) or changed
    if pressed("rotate") then changed = board:rotate() or changed end
    if pressed("hold") and board:hold() then
      gravity = 0
      draw_status()
      changed = true
    end
    local drop = pressed("hard_drop")
    if drop and not board.game_over then
      local distance, cleared, cleared_rows = board:hard_drop()
      award(cleared, distance, cleared_rows)
      gravity = 0
      changed = true
    end
    if changed then draw_board() end
  end,
  draw = function() assert(board and rendered and status) end,
}
