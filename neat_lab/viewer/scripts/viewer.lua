-- Replays NEAT training generations written by neat_lab/trainer/train.py.
-- Reads runs/ (a live or finished training run) and falls back to the
-- checked-in demo/ run. Each generation file holds the champion and the best
-- lander of other species, recorded at 30 Hz by the C++ runner.
local tp = require("acuppa")
local Replay = require("scripts.replay")
local Panel = require("scripts.net_panel")

local SOURCES = {"runs", "demo"}
local POLL_SECONDS = 1.0
local HOLD_SECONDS = 1.5
local SLOTS = 8
local GHOST_COLORS = 7
local GHOST_SPACING = 0.9
local UI_BUDGET = 250

local slots = {}
local held = {}
local source, status, run, problem
local index, live, show_net, speed = 0, true, true, 1
local clock, poll = 0, 0

local function pressed(action)
  local down = tp.action(action) > 0
  local edge = down and not held[action]
  held[action] = down
  return edge
end

local function read(path)
  local ok, value = pcall(tp.data_read, path)
  if not ok then return nil, tostring(value) end
  return value
end

local function find_status()
  for _, dir in ipairs(SOURCES) do
    local value, err = read(dir .. "/status.lua")
    if err then return dir, nil, err end
    if value then
      if value.format ~= 1 or type(value.generations) ~= "table" or #value.generations == 0 then
        return dir, nil, dir .. "/status.lua is not a run status file"
      end
      return dir, value
    end
  end
end

local function decode(data)
  if data.format ~= 1 then error("unsupported run format " .. tostring(data.format)) end
  if type(data.landers) ~= "table" or #data.landers == 0 then error("run has no landers") end
  for i, lander in ipairs(data.landers) do
    lander.track = Replay.decode(lander.frames, 8, "lander " .. i .. " frames")
  end
  data.acts = Replay.decode(data.champion_acts, #data.node_keys, "champion activations")
  data.duration = 0
  for _, lander in ipairs(data.landers) do
    data.duration = math.max(data.duration, Replay.duration(lander.track))
  end
  return data
end

local function hide(slot)
  for _, part in ipairs({slot.hull, slot.cabin, slot.main, slot.left, slot.right}) do
    tp.component_set(part, "mesh", {visible = false})
  end
end

local function load_generation(new_index)
  local generation = status.generations[new_index]
  local data, err = read(string.format("%s/gen_%04d.lua", source, generation))
  if not data then
    problem = err or string.format("%s/gen_%04d.lua is missing", source, generation)
    return false
  end
  local ok, result = pcall(decode, data)
  if not ok then
    problem = string.format("gen_%04d.lua: %s", generation, tostring(result))
    return false
  end
  run, index, clock, problem = result, new_index, 0, nil
  for i, slot in ipairs(slots) do
    local lander = run.landers[i]
    if lander then
      local look = i == 1 and "assets/champion.gltf" or
                   string.format("assets/ghost_%d.gltf", (lander.species - 1) % GHOST_COLORS + 1)
      tp.component_set(slot.hull, "mesh", {mesh = look, visible = true})
      tp.component_set(slot.cabin, "mesh", {visible = true})
    else
      hide(slot)
    end
  end
  return true
end

local function refresh_status()
  local dir, value, err = find_status()
  if err then
    problem = err
    return
  end
  if not value then
    source, status, run = nil, nil, nil
    return
  end
  local switched = dir ~= source
  source, status = dir, value
  if switched or not run then
    for _, slot in ipairs(slots) do hide(slot) end
    run = nil
    load_generation(#status.generations)
  end
end

local function flame(part, length, position, scale)
  if length < 0.03 then
    tp.component_set(part, "mesh", {visible = false})
  else
    tp.component_set(part, "mesh", {visible = true})
    tp.component_set(part, "transform", {position = position, scale = scale})
  end
end

local function pose(slot, frame, z)
  local x, y, rx, ry, rz, main, side = table.unpack(frame)
  tp.component_set(slot.root, "transform", {position = {x, y, z}, rotation = {rx, ry, rz}})
  local m = 0.45 * main
  flame(slot.main, m, {0, -0.3 - m, 0}, {0.14, m, 0.14})
  local s = 0.3 * math.abs(side)
  -- Positive side thrust pushes toward +X, so its exhaust points toward -X.
  flame(slot.left, side > 0 and s or 0, {-0.45 - s, 0, 0}, {s, 0.07, 0.07})
  flame(slot.right, side < 0 and s or 0, {0.45 + s, 0, 0}, {s, 0.07, 0.07})
end

local function make_slot(i)
  local root = tp.entity_create(string.format("lander %d", i))
  tp.component_set(root, "transform", {position = {0, -20, 0}})
  local function part(name, mesh, position, scale)
    local id = tp.entity_create(string.format("lander %d %s", i, name))
    tp.component_set(id, "transform", {parent = root, position = position, scale = scale})
    tp.component_set(id, "mesh", {mesh = mesh, visible = false})
    return id
  end
  return {
    root = root,
    hull = part("hull", "assets/champion.gltf", {0, 0, 0}, {0.45, 0.3, 0.45}),
    cabin = part("cabin", "assets/cabin.gltf", {0, 0.4, 0}, {0.22, 0.1, 0.22}),
    main = part("main jet", "assets/flame.gltf", {0, -0.5, 0}, {0.14, 0.2, 0.14}),
    left = part("left jet", "assets/flame.gltf", {-0.6, 0, 0}, {0.15, 0.07, 0.07}),
    right = part("right jet", "assets/flame.gltf", {0.6, 0, 0}, {0.15, 0.07, 0.07}),
  }
end

local function step_generation(delta)
  if not status then return end
  local target = math.max(1, math.min(#status.generations, index + delta))
  if target ~= index then load_generation(target) end
end

local function draw_graph(x, y, width, height)
  local history = run.history
  tp.ui_rect(x, y, width, height, 0.02, 0.03, 0.06, 0.8)
  tp.ui_text(x + 10, y + 8, "BEST FITNESS BY GENERATION", 1)
  local bars = math.min(#history, 50)
  local items = 2
  if bars == 0 then return items end
  local per = #history / bars
  local peak = 230
  local bar_width = (width - 20) / bars
  for b = 1, bars do
    local value = 0
    for i = math.floor((b - 1) * per) + 1, math.max(math.floor((b - 1) * per) + 1, math.floor(b * per)) do
      value = math.max(value, history[i] or 0)
    end
    local h = math.max(1, (height - 34) * math.min(1, value / peak))
    local landed = value >= 100
    tp.ui_rect(x + 10 + (b - 1) * bar_width, y + height - 8 - h, math.max(1, bar_width - 1), h,
               landed and 0.25 or 0.85, landed and 0.85 or 0.45, landed and 0.4 or 0.2, 0.9)
    items = items + 1
  end
  return items
end

return {
  start = function()
    tp.set_exposure(1.0)
    tp.set_environment("assets/night.hdr", 0.45)
    tp.set_sky({zenith = {0.01, 0.015, 0.05}, horizon = {0.12, 0.13, 0.22},
                ground = {0.05, 0.05, 0.06}, sun_direction = {0.35, 1, 0.45},
                sun_color = {0.8, 0.85, 1}, sun_size = 0.02})
    tp.set_ssao(2, 0.4, 1.2)
    for i = 1, SLOTS do slots[i] = make_slot(i) end
    refresh_status()
  end,

  fixed_update = function() end,

  update = function(dt)
    if pressed("exit") then tp.quit() end
    if pressed("toggle_net") then show_net = not show_net end
    if pressed("speed") then speed = speed == 1 and 3 or 1 end
    if pressed("previous") then live = false; step_generation(-1) end
    if pressed("next") then live = false; step_generation(1) end
    if pressed("live") then
      live = true
      refresh_status()
      if status then load_generation(#status.generations) end
    end
    if pressed("replay") then clock = 0 end

    poll = poll + dt
    if poll >= POLL_SECONDS then
      poll = 0
      refresh_status()
    end
    if not run then return end
    clock = clock + dt * speed
    if clock > run.duration + HOLD_SECONDS then
      if live and status and index < #status.generations then
        load_generation(#status.generations)
      else
        clock = 0
      end
    end
    for i, slot in ipairs(slots) do
      local lander = run.landers[i]
      if lander then pose(slot, (Replay.sample(lander.track, clock)), -(i - 1) * GHOST_SPACING) end
    end
  end,

  draw = function()
    local w, h = tp.viewport_size()
    local items = 0
    tp.ui_rect(0, 0, w, 56, 0.02, 0.03, 0.06, 0.85)
    items = items + 1
    if tp.ui_button("net", w - 190, 10, 178, 36, show_net and "HIDE NETWORK" or "SHOW NETWORK") then
      show_net = not show_net
    end
    items = items + 1
    if not run then
      tp.ui_text(16, 10, "NEAT LANDER", 2)
      tp.ui_text(w / 2 - 300, h / 2 - 30, problem and "RUN FILE ERROR" or "WAITING FOR TRAINING", 3)
      tp.ui_text(w / 2 - 300, h / 2 + 10, problem or
                 "START neat_lab/trainer/train.py TO WATCH LANDERS EVOLVE", 1)
      return
    end
    local mode = live and "LIVE" or string.format("BROWSING %d/%d", index, #status.generations)
    if source == "demo" then mode = mode .. " (DEMO RUN)" end
    tp.ui_text(16, 8, string.format("NEAT LANDER  GEN %d  SCENARIO %d/%d  SPECIES %d  %s",
                                    run.generation, run.scenario, run.scenarios,
                                    run.species_count, mode), 2)
    local champion = run.landers[1]
    tp.ui_text(16, 34, string.format(
      "CHAMPION %s (IMPACT %.1f M/S)  LANDED %d/%d  BEST %.1f  MEAN %.1f  POP %d  SPEED %dX",
      string.upper(string.gsub(champion.outcome, "_", " ")), champion.impact, run.landed,
      run.scenarios, run.best_fitness, run.mean_fitness, run.population, speed), 1)
    tp.ui_text(16, h - 22, "N NETWORK   LEFT/RIGHT GENERATION   L LIVE   R REPLAY   F SPEED   ESC QUIT", 1)
    items = items + 3
    if problem then
      tp.ui_text(16, 64, "RUN FILE ERROR: " .. problem, 1)
      items = items + 1
    end
    items = items + draw_graph(16, h - 140, 320, 108)
    if show_net then
      local values = {}
      local frame = run.acts[math.min(#run.acts, math.floor(clock * Replay.FPS) + 1)]
      for i, key in ipairs(run.node_keys) do values[key] = frame[i] end
      Panel.draw(tp, run.network, values, w - 442, 66, 430, 280, UI_BUDGET - items)
    end
  end,
}
