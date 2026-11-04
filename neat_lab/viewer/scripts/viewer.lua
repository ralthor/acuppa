-- Replays NEAT training generations written by neat_lab/trainer/train.py.
-- Reads runs/ (a live or finished training run) and falls back to the
-- checked-in demo/ run. Each generation file holds the champion and the best
-- lander of other species, recorded at 30 Hz by the C++ runner on one of that
-- generation's random scenarios. Landers that fail are blown up where their
-- flight ended; landers that land light a green beacon.
local tp = require("acuppa")
local Replay = require("scripts.replay")
local Panel = require("scripts.net_panel")
local Effects = require("scripts.effects")

local SOURCES = {"runs", "demo"}
local POLL_SECONDS = 1.0
local HOLD_SECONDS = 2.5
local SLOTS = 8
local GHOST_COLORS = 7
local GHOST_SPACING = 0.9
local UI_BUDGET = 250
local FRAME_WIDTH = 9  -- x y rx ry rz main side fuel wind
local CAMERA = {eye = {5.5, 9, 22}, target = {5.5, 5, -2}, up = {0, 1, 0}, fov = 50,
                near = 0.1, far = 200}
local OUTCOME_TEXT = {landed = "LANDED", off_pad = "MISSED PAD", crashed = "CRASHED",
                      tipped = "TIPPED OVER", timeout = "OUT OF TIME",
                      out_of_bounds = "FLEW AWAY"}

local slots, streaks = {}, {}
local held = {}
local source, status, run, problem
local index, live, show_net, show_rules, speed = 0, true, true, false, 1
local clock, poll, wind_offset = 0, 0, 0

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
  if data.format ~= 2 then error("unsupported run format " .. tostring(data.format)) end
  if type(data.landers) ~= "table" or #data.landers == 0 then error("run has no landers") end
  for i, lander in ipairs(data.landers) do
    lander.track = Replay.decode(lander.frames, FRAME_WIDTH, "lander " .. i .. " frames")
    lander.ends = Replay.duration(lander.track)
  end
  data.acts = Replay.decode(data.champion_acts, #data.node_keys, "champion activations")
  data.duration = 0
  for _, lander in ipairs(data.landers) do data.duration = math.max(data.duration, lander.ends) end
  return data
end

local function show(part, visible)
  tp.component_set(part, "mesh", {visible = visible})
end

-- Puts every lander back together for the start of a replay.
local function reset_slot(slot, lander)
  slot.exploded, slot.chimed = false, false
  for _, piece in ipairs(slot.debris) do show(piece, false) end
  show(slot.flash, false)
  show(slot.beacon, false)
  for _, part in ipairs({slot.hull, slot.cabin}) do show(part, lander ~= nil) end
  if not lander then
    for _, part in ipairs({slot.main, slot.left, slot.right}) do show(part, false) end
  end
end

local function restart_replay()
  clock, wind_offset = 0, 0
  for i, slot in ipairs(slots) do reset_slot(slot, run and run.landers[i]) end
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
  run, index, problem = result, new_index, nil
  for i, slot in ipairs(slots) do
    local lander = run.landers[i]
    if lander then
      local look = i == 1 and "assets/champion.gltf" or
                   string.format("assets/ghost_%d.gltf", (lander.species - 1) % GHOST_COLORS + 1)
      tp.component_set(slot.hull, "mesh", {mesh = look})
      for p, piece in ipairs(slot.debris) do
        tp.component_set(piece, "mesh", {mesh = p % 2 == 0 and "assets/scorch.gltf" or look})
      end
    end
  end
  restart_replay()
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
    for _, slot in ipairs(slots) do reset_slot(slot, nil) end
    return
  end
  local switched = dir ~= source
  source, status = dir, value
  if switched or not run then
    run = nil
    for _, slot in ipairs(slots) do reset_slot(slot, nil) end
    load_generation(#status.generations)
  end
end

local function flame(part, length, position, scale)
  if length < 0.03 then
    show(part, false)
  else
    show(part, true)
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

local function explode(slot, frame, z, champion)
  slot.exploded = true
  slot.blast = {frame[1], frame[2], z}
  for _, part in ipairs({slot.hull, slot.cabin, slot.main, slot.left, slot.right}) do
    show(part, false)
  end
  for _, piece in ipairs(slot.debris) do show(piece, true) end
  tp.sound_play("assets/boom.wav", {position = slot.blast, volume = champion and 0.9 or 0.35})
end

local function animate_wreck(slot, i, since)
  for p, piece in ipairs(slot.debris) do
    local position, rotation = Effects.debris(slot.blast, i, p, since)
    tp.component_set(piece, "transform", {position = position, rotation = rotation})
  end
  local size = Effects.flash(since)
  if size then
    show(slot.flash, true)
    tp.component_set(slot.flash, "transform", {position = slot.blast, scale = {size, size, size}})
  else
    show(slot.flash, false)
  end
end

local function make_slot(i)
  local root = tp.entity_create(string.format("lander %d", i))
  tp.component_set(root, "transform", {position = {0, -20, 0}})
  local function part(name, mesh, position, scale, parent)
    local id = tp.entity_create(string.format("lander %d %s", i, name))
    tp.component_set(id, "transform", {parent = parent, position = position, scale = scale})
    tp.component_set(id, "mesh", {mesh = mesh, visible = false})
    return id
  end
  local slot = {
    root = root,
    hull = part("hull", "assets/champion.gltf", {0, 0, 0}, {0.45, 0.3, 0.45}, root),
    cabin = part("cabin", "assets/cabin.gltf", {0, 0.4, 0}, {0.22, 0.1, 0.22}, root),
    beacon = part("beacon", "assets/beacon.gltf", {0, 0.62, 0}, {0.08, 0.12, 0.08}, root),
    main = part("main jet", "assets/flame.gltf", {0, -0.5, 0}, {0.14, 0.2, 0.14}, root),
    left = part("left jet", "assets/flame.gltf", {-0.6, 0, 0}, {0.15, 0.07, 0.07}, root),
    right = part("right jet", "assets/flame.gltf", {0.6, 0, 0}, {0.15, 0.07, 0.07}, root),
    flash = part("flash", "assets/flame.gltf", {0, -20, 0}, {1, 1, 1}, 0),
    debris = {},
  }
  for p = 1, Effects.PIECES do
    local size = 0.08 + (p % 3) * 0.05
    slot.debris[p] = part("debris " .. p, "assets/champion.gltf", {0, -20, 0},
                          {size, size * 0.7, size}, 0)
  end
  return slot
end

local function step_generation(delta)
  if not status then return end
  local target = index + delta
  if target > #status.generations then
    live = true  -- Stepping past the newest generation returns to live mode.
    return
  end
  live = false
  target = math.max(1, target)
  if target ~= index then load_generation(target) end
end

-- Screen position of a world point for the scene camera, or nil behind it.
local function project(point, width, height)
  local e, t = CAMERA.eye, CAMERA.target
  local f = {t[1] - e[1], t[2] - e[2], t[3] - e[3]}
  local fl = math.sqrt(f[1] ^ 2 + f[2] ^ 2 + f[3] ^ 2)
  f = {f[1] / fl, f[2] / fl, f[3] / fl}
  local r = {f[2] * CAMERA.up[3] - f[3] * CAMERA.up[2], f[3] * CAMERA.up[1] - f[1] * CAMERA.up[3],
             f[1] * CAMERA.up[2] - f[2] * CAMERA.up[1]}
  local rl = math.sqrt(r[1] ^ 2 + r[2] ^ 2 + r[3] ^ 2)
  r = {r[1] / rl, r[2] / rl, r[3] / rl}
  local u = {r[2] * f[3] - r[3] * f[2], r[3] * f[1] - r[1] * f[3], r[1] * f[2] - r[2] * f[1]}
  local d = {point[1] - e[1], point[2] - e[2], point[3] - e[3]}
  local depth = d[1] * f[1] + d[2] * f[2] + d[3] * f[3]
  if depth < CAMERA.near then return nil end
  local half = math.tan(math.rad(CAMERA.fov) / 2)
  local sx = (d[1] * r[1] + d[2] * r[2] + d[3] * r[3]) / (depth * half * width / height)
  local sy = (d[1] * u[1] + d[2] * u[2] + d[3] * u[3]) / (depth * half)
  return width / 2 * (1 + sx), height / 2 * (1 - sy)
end

local function wind_text(wind)
  local arrows = string.rep(wind >= 0 and ">" or "<", math.max(1, math.floor(math.abs(wind) + 0.5)))
  return string.format("WIND %s %+.1f M/S2", arrows, wind)
end

local function draw_graph(x, y, width, height)
  local history = run.rate_history or {}
  tp.ui_rect(x, y, width, height, 0.02, 0.03, 0.06, 0.8)
  tp.ui_text(x + 10, y + 8, string.format("POPULATION LANDING RATE  NOW %.0f%%",
                                          100 * (run.landing_rate or 0)), 1)
  local bars = math.min(#history, 50)
  local items = 2
  if bars == 0 then return items end
  local per = #history / bars
  local peak = 0.05
  for _, value in ipairs(history) do peak = math.max(peak, value) end
  local bar_width = (width - 20) / bars
  for b = 1, bars do
    local value = 0
    for i = math.floor((b - 1) * per) + 1, math.max(math.floor((b - 1) * per) + 1, math.floor(b * per)) do
      value = math.max(value, history[i] or 0)
    end
    local h = math.max(1, (height - 34) * value / peak)
    tp.ui_rect(x + 10 + (b - 1) * bar_width, y + height - 8 - h, math.max(1, bar_width - 1), h,
               0.25, 0.85, 0.4, 0.9)
    items = items + 1
  end
  return items
end

local RULES = {
  "HOW A FLIGHT IS SCORED (MAX ABOUT 260)",
  "CENTER   UP TO 60 FOR ENDING CLOSE TO THE PAD",
  "LANDING  100 LANDED, 40 MISSED PAD, 20 TIPPED, 0-20 CRASH",
  "SOFT     UP TO 30 FOR TOUCHING DOWN BELOW 1.5 M/S",
  "FUEL     UP TO 40 FOR FUEL LEFT AFTER LANDING",
  "SPEED    UP TO 30 FOR LANDING SOONER (10 S LIMIT)",
  "OUT OF TIME: HALF CENTER ONLY.  FLEW AWAY: 0",
  "FITNESS = MEAN OVER 8 RANDOM FLIGHTS, NEW WIND EACH GEN",
}

return {
  start = function()
    tp.set_camera(CAMERA)
    tp.sound_set_listener(CAMERA.eye, {0, -0.17, -1}, {0, 1, 0})
    tp.set_exposure(1.0)
    tp.set_environment("assets/night.hdr", 0.45)
    tp.set_sky({zenith = {0.01, 0.015, 0.05}, horizon = {0.12, 0.13, 0.22},
                ground = {0.05, 0.05, 0.06}, sun_direction = {0.35, 1, 0.45},
                sun_color = {0.8, 0.85, 1}, sun_size = 0.02})
    tp.set_ssao(2, 0.4, 1.2)
    for i = 1, SLOTS do slots[i] = make_slot(i) end
    for i = 1, Effects.STREAKS do
      streaks[i] = tp.entity_create("wind streak " .. i)
      tp.component_set(streaks[i], "transform", {position = {0, -20, 0}, scale = {0.3, 0.015, 0.015}})
      tp.component_set(streaks[i], "mesh", {mesh = "assets/streak.gltf", visible = false,
                                            casts_shadows = false})
    end
    refresh_status()
  end,

  fixed_update = function() end,

  update = function(dt)
    if pressed("exit") then tp.quit() end
    if pressed("toggle_net") then show_net = not show_net end
    if pressed("rules") then show_rules = not show_rules end
    if pressed("speed") then speed = speed == 1 and 3 or 1 end
    if pressed("previous") then step_generation(-1) end
    if pressed("next") then step_generation(1) end
    local go_live = pressed("live")
    if go_live then live = true end
    if pressed("replay") and run then restart_replay() end
    -- In live mode, jump to the newest generation as soon as the replay ends.
    if go_live or (live and status and run and index < #status.generations and
                   clock > run.duration + HOLD_SECONDS) then
      refresh_status()
      if status then load_generation(#status.generations) end
    end

    poll = poll + dt
    if poll >= POLL_SECONDS then
      poll = 0
      refresh_status()
    end
    if not run then
      for _, streak in ipairs(streaks) do show(streak, false) end
      return
    end
    local step = dt * speed
    clock = clock + step
    if clock > run.duration + HOLD_SECONDS and not (live and index < #status.generations) then
      restart_replay()
    end

    local wind = run.weather.wind
    for i, slot in ipairs(slots) do
      local lander = run.landers[i]
      if lander then
        local z = -(i - 1) * GHOST_SPACING
        local frame = Replay.sample(lander.track, clock)
        if i == 1 then wind = frame[9] end
        if lander.outcome ~= "landed" and clock >= lander.ends then
          if not slot.exploded then explode(slot, frame, z, i == 1) end
          animate_wreck(slot, i, clock - lander.ends)
        elseif not slot.exploded then
          pose(slot, frame, z)
          local down = lander.outcome == "landed" and clock >= lander.touch_time
          show(slot.beacon, down and math.floor(clock * 4) % 2 == 0)
          if down and not slot.chimed then
            slot.chimed = true
            tp.sound_play("assets/chime.wav", {position = {frame[1], frame[2], z},
                                               volume = i == 1 and 0.8 or 0.3})
          end
        end
      end
    end

    wind_offset = wind_offset + wind * 2.5 * step
    for i, streak in ipairs(streaks) do
      local position, half = Effects.streak(i, wind_offset, wind)
      show(streak, position ~= nil)
      if position then
        tp.component_set(streak, "transform", {position = position, scale = {half, 0.015, 0.015}})
      end
    end
  end,

  draw = function()
    local w, h = tp.viewport_size()
    local items = 0
    local function text(...)
      items = items + 1
      tp.ui_text(...)
    end
    local function rect(...)
      items = items + 1
      tp.ui_rect(...)
    end
    rect(0, 0, w, 56, 0.02, 0.03, 0.06, 0.85)
    items = items + 1
    if tp.ui_button("net", w - 190, 10, 178, 36, show_net and "HIDE NETWORK" or "SHOW NETWORK") then
      show_net = not show_net
    end
    if not run then
      text(16, 10, "NEAT LANDER", 2)
      text(w / 2 - 300, h / 2 - 30, problem and "RUN FILE ERROR" or "WAITING FOR TRAINING", 3)
      text(w / 2 - 300, h / 2 + 10, problem or
           "START neat_lab/trainer/train.py TO WATCH LANDERS EVOLVE", 1)
      return
    end

    local newest = status.generations[#status.generations]
    text(16, 8, string.format("NEAT LANDER  GEN %d  FLIGHT %d/%d  SPECIES %d  POP %d",
                              run.generation, run.scenario, run.scenarios, run.species_count,
                              run.population), 2)
    local state = status.running and string.format("TRAINING IS AT GEN %d", newest) or
                  string.format("TRAINING FINISHED AT GEN %d", newest)
    if source == "demo" then state = "DEMO RUN, " .. state end
    local mode = live and "LIVE: SHOWING THE NEWEST GENERATION" or
                 string.format("REWATCHING GEN %d (%d OF %d LISTED)", run.generation, index,
                               #status.generations)
    text(16, 34, string.format("%s.  %s.  REPLAY SPEED %dX", mode, state, speed), 1)

    local champion = run.landers[1]
    local frame = Replay.sample(champion.track, clock)
    text(16, 66, string.format("%s   GUSTS +-%.1f EVERY %.1f S", wind_text(frame[9]),
                               run.weather.gust, run.weather.gust_period), 1)
    local sc = champion.score
    local how = champion.outcome == "landed" and
                string.format("LANDED IN %.1f S AT %.1f M/S", champion.touch_time, champion.impact) or
                OUTCOME_TEXT[champion.outcome] or champion.outcome
    text(16, 82, string.format("CHAMPION THIS FLIGHT: %s.  SCORE %.0f = CENTER %.0f + LANDING %.0f"
                               .. " + SOFT %.0f + FUEL %.0f + SPEED %.0f", how, champion.run_score,
                               sc.center, sc.landing, sc.soft, sc.fuel, sc.speed), 1)
    text(16, 98, string.format("CHAMPION FITNESS %.1f (MEAN OF %d FLIGHTS), LANDED %d/%d.  "
                               .. "POPULATION MEAN %.1f", run.best_fitness, run.scenarios,
                               run.landed, run.scenarios, run.mean_fitness), 1)
    if not live then
      text(w / 2 - 340, 124, "REWATCHING AN OLDER GENERATION - RIGHT: NEWER   L: BACK TO LIVE", 1)
    end
    if problem then text(16, 124, "RUN FILE ERROR: " .. problem, 1) end

    for i, lander in ipairs(run.landers) do
      local f = Replay.sample(lander.track, clock)
      local done = clock >= (lander.outcome == "landed" and lander.touch_time or lander.ends)
      local label = done and (OUTCOME_TEXT[lander.outcome] or "") or (i == 1 and "CHAMPION" or nil)
      if label then
        local sx, sy = project({f[1], f[2] + 1.1, -(i - 1) * GHOST_SPACING}, w, h)
        if sx and sx > 0 and sx < w and sy > 60 and sy < h then
          text(sx - #label * 3, sy, label, 1)
        end
      end
    end

    if show_rules then
      rect(16, 140, 420, 8 * 16 + 16, 0.02, 0.03, 0.06, 0.88)
      for i, line in ipairs(RULES) do text(26, 148 + (i - 1) * 16, line, 1) end
    end
    text(16, h - 22, "N NETWORK  H SCORING  LEFT/RIGHT OLDER/NEWER GEN  L LIVE  R REPLAY  "
                     .. "F SPEED  ESC QUIT", 1)
    items = items + draw_graph(16, h - 140, 320, 108)
    if show_net then
      local values = {}
      local acts = run.acts[math.min(#run.acts, math.floor(clock * Replay.FPS) + 1)]
      for i, key in ipairs(run.node_keys) do values[key] = acts[i] end
      Panel.draw(tp, run.network, values, w - 442, 66, 430, 280, UI_BUDGET - items)
    end
  end,
}
