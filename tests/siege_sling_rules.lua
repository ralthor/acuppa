local Rules = assert(loadfile(rules_file))()

local passed = 0
local function near(a, b, tolerance)
  return math.abs(a - b) <= (tolerance or 1e-9)
end
local function check(name, condition)
  if not condition then error("check failed: " .. name, 2) end
  passed = passed + 1
end
local function fails(name, call, pattern)
  local ok, message = pcall(call)
  if ok then error("expected failure: " .. name, 2) end
  if pattern and not tostring(message):find(pattern, 1, true) then
    error(name .. ": wrong message: " .. tostring(message), 2)
  end
  passed = passed + 1
end

-- Module shape: pure, no globals introduced.
check("no globals", rawget(_G, "Rules") == nil and rawget(_G, "Game") == nil)

-- Config ------------------------------------------------------------------
local cfg = Rules.config()
check("defaults", cfg.yaw_limit == 45 and cfg.pitch_min == 5 and cfg.pitch_max == 70
  and cfg.gravity == -9.81 and cfg.settle_ticks == 30)
local custom = Rules.config({max_drag = 100, cancel_drag = 10})
check("override", custom.max_drag == 100 and custom.yaw_limit == 45)
fails("config type", function() Rules.config(5) end, "overrides")
fails("config unknown", function() Rules.config({yaw = 1}) end, "unknown config field yaw")
fails("config nan", function() Rules.config({gravity = 0 / 0}) end, "gravity")
fails("config gravity sign", function() Rules.config({gravity = 9.81}) end, "negative")
fails("config pitch order", function() Rules.config({pitch_min = 50, pitch_max = 40}) end, "pitch")
fails("config drag", function() Rules.config({cancel_drag = 300}) end, "cancel_drag")
fails("config speeds", function() Rules.config({min_speed = 30}) end, "speeds")
fails("config ticks", function() Rules.config({settle_ticks = 2.5}) end, "settle_ticks")
fails("config timeouts", function() Rules.config({settle_timeout = 0}) end, "timeouts")
fails("config yaw limit", function() Rules.config({yaw_limit = 120}) end, "yaw_limit")
fails("bad config passed", function() Rules.default_aim({}) end, "config")

local rest = Rules.default_aim()
check("default aim", rest.yaw == 0 and rest.pitch == 37.5 and rest.power == 0.5)

-- Drag mapping ------------------------------------------------------------
local straight = Rules.aim_from_drag(0, 240)
check("full pull down", straight.yaw == 0 and straight.pitch == 70 and straight.power == 1)
local half = Rules.aim_from_drag(0, 120)
check("half pull down", half.yaw == 0 and near(half.pitch, 37.5) and near(half.power, 0.5))
local left_pull = Rules.aim_from_drag(-120, 0)
check("pull left fires right", near(left_pull.yaw, 22.5) and left_pull.pitch == 5
  and near(left_pull.power, 0.5))
local right_pull = Rules.aim_from_drag(120, 0)
check("pull right fires left", near(right_pull.yaw, -22.5))
local far = Rules.aim_from_drag(-1000, 1000)
check("clamped drag", far.yaw == 45 and far.pitch == 70 and far.power == 1)
local up = Rules.aim_from_drag(0, -200)
check("pull up keeps min pitch", up.pitch == 5 and near(up.power, 200 / 240))
local cancelled, reason = Rules.aim_from_drag(10, 10)
check("tiny drag cancels", cancelled == nil and reason == "cancel")
check("zero drag cancels", Rules.aim_from_drag(0, 0) == nil)
local edge = Rules.aim_from_drag(0, 18)
check("cancel boundary fires", edge ~= nil and near(edge.power, 18 / 240))
check("custom config drag", Rules.aim_from_drag(0, 100, custom).power == 1)
fails("drag nan", function() Rules.aim_from_drag(0 / 0, 4) end, "dx")
fails("drag inf", function() Rules.aim_from_drag(1, math.huge) end, "dy")
fails("drag string", function() Rules.aim_from_drag("1", 4) end, "dx")

-- Key aim -------------------------------------------------------------------
local keyed = Rules.adjust_aim(rest, {yaw = 1, pitch = -1, power = 1}, 0.25)
check("key aim rates", near(keyed.yaw, 15) and near(keyed.pitch, 26.25) and near(keyed.power, 0.7))
check("key aim pure", rest.yaw == 0 and rest.power == 0.5)
local pinned = rest
for _ = 1, 100 do pinned = Rules.adjust_aim(pinned, {yaw = -1, pitch = 1, power = -1}, 0.1) end
check("key aim clamps", pinned.yaw == -45 and pinned.pitch == 70 and pinned.power == 0)
check("key aim empty input", Rules.adjust_aim(rest, {}, 0.1).pitch == 37.5)
check("key aim zero dt", Rules.adjust_aim(rest, {yaw = 1}, 0).yaw == 0)
fails("key axis range", function() Rules.adjust_aim(rest, {yaw = 2}, 0.1) end, "input.yaw")
fails("key axis unknown", function() Rules.adjust_aim(rest, {roll = 1}, 0.1) end, "roll")
fails("key dt negative", function() Rules.adjust_aim(rest, {}, -0.1) end, "dt")
fails("key dt huge", function() Rules.adjust_aim(rest, {}, 1) end, "dt")
fails("key aim out of range", function()
  Rules.adjust_aim({yaw = 90, pitch = 10, power = 0.5}, {}, 0.1) end, "aim.yaw")
fails("key input type", function() Rules.adjust_aim(rest, nil, 0.1) end, "input")

-- Launch velocity -----------------------------------------------------------
local v = Rules.launch_velocity({yaw = 0, pitch = 5, power = 0})
check("forward is -Z", near(v[1], 0) and v[3] < 0 and near(v[2], 8 * math.sin(math.rad(5))))
check("min speed", near(math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2), 8))
v = Rules.launch_velocity({yaw = 45, pitch = 5, power = 1})
check("yaw right is +X", v[1] > 0 and v[3] < 0 and near(v[1], -v[3]))
check("max speed", near(math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2), 28))
v = Rules.launch_velocity({yaw = -30, pitch = 60, power = 0.5})
local speed = 18
check("known direction", near(v[1], speed * 0.5 * -0.5, 1e-9)
  and near(v[2], speed * math.sqrt(3) / 2, 1e-9)
  and near(v[3], -speed * 0.5 * math.sqrt(3) / 2, 1e-9))
fails("velocity power range", function()
  Rules.launch_velocity({yaw = 0, pitch = 30, power = 1.5}) end, "aim.power")
fails("velocity pitch range", function()
  Rules.launch_velocity({yaw = 0, pitch = 80, power = 0.5}) end, "aim.pitch")
fails("velocity aim type", function() Rules.launch_velocity(3) end, "aim")

-- Arc prediction --------------------------------------------------------------
local origin = {0, 2, 0}
local launch = {3, 10, -12}
local arc, landing = Rules.predict_arc(origin, launch, {step = 1 / 60, max_points = 1000})
local g = -9.81
check("arc starts at origin", arc[1][1] == 0 and arc[1][2] == 2 and arc[1][3] == 0)
local apex = 0
for _, point in ipairs(arc) do apex = math.max(apex, point[2]) end
local closed_apex = 2 + 10 * 10 / (2 * 9.81)
check("arc apex", apex <= closed_apex + 1e-9 and closed_apex - apex < 0.01)
local t_land = (10 + math.sqrt(100 + 2 * 9.81 * 2)) / 9.81
check("arc lands", landing ~= nil and arc[#arc] == landing)
check("landing point", near(landing[2], 0) and near(landing[1], 3 * t_land, 1e-9)
  and near(landing[3], -12 * t_land, 1e-9))
local mid = arc[31]
check("arc sample", near(mid[2], 2 + 10 * 0.5 + 0.5 * g * 0.25, 1e-9) and near(mid[1], 1.5, 1e-9))
local short, none = Rules.predict_arc(origin, launch, {max_points = 5})
check("point cap", #short == 5 and none == nil)
local flat, flat_land = Rules.predict_arc({0, 0, 0}, {0, 0, -5}, {step = 0.1})
check("ground start", #flat == 2 and near(flat_land[3], 0))
local skyward = Rules.predict_arc(origin, launch, {ground_y = false, max_points = 400})
check("no ground", #skyward == 400 and skyward[400][2] < -100)
local raised, raised_land = Rules.predict_arc({0, 5, 0}, {0, 0, -1}, {ground_y = 3, step = 0.05})
check("ground height", near(raised_land[2], 3) and near(raised_land[3], -math.sqrt(4 / 9.81), 1e-9)
  and #raised > 2)
local lunar = Rules.predict_arc({0, 0, 0}, {0, 1, 0}, {gravity = -1, step = 0.5, ground_y = -1})
check("gravity option", near(lunar[3][2], 1 - 0.5))
fails("arc origin", function() Rules.predict_arc({0, 0}, launch) end, "origin")
fails("arc velocity", function() Rules.predict_arc(origin, {0, 0 / 0, 0}) end, "velocity")
fails("arc step", function() Rules.predict_arc(origin, launch, {step = 0}) end, "step")
fails("arc points", function() Rules.predict_arc(origin, launch, {max_points = 1}) end, "max_points")
fails("arc points integer", function() Rules.predict_arc(origin, launch, {max_points = 2.5}) end,
  "integer")
fails("arc below ground", function() Rules.predict_arc({0, -1, 0}, launch) end, "below")
fails("arc option", function() Rules.predict_arc(origin, launch, {steps = 1}) end, "steps")
fails("arc options type", function() Rules.predict_arc(origin, launch, 4) end, "options")

-- Materials and damage --------------------------------------------------------
local M = Rules.MATERIALS
check("materials", M.wood and M.stone and M.glass)
check("glass fragile", M.glass.health < M.wood.health and M.glass.min_speed < M.wood.min_speed)
check("stone tough", M.stone.health > M.wood.health and M.stone.min_speed > M.wood.min_speed)
for name, material in pairs(M) do
  check(name .. " physics", material.density > 0 and material.friction >= 0
    and material.restitution >= 0 and material.restitution <= 1 and material.points > 0)
  check(name .. " below threshold", Rules.impact_damage(name, material.min_speed) == 0
    and Rules.impact_damage(name, material.min_speed * 0.5) == 0
    and Rules.impact_damage(name, 0) == 0)
  check(name .. " above threshold", near(Rules.impact_damage(name, material.min_speed + 1), 20))
end
check("glass breaks at 3 m/s", Rules.impact_damage("glass", 3) >= M.glass.health)
check("stone survives 10 m/s", Rules.impact_damage("stone", 10) < M.stone.health)
check("custom damage rate", near(Rules.impact_damage("wood", 3,
  Rules.config({damage_per_speed = 5})), 5))
fails("damage material", function() Rules.impact_damage("lava", 3) end, "unknown material lava")
fails("damage material type", function() Rules.impact_damage(1, 3) end, "material")
fails("damage speed", function() Rules.impact_damage("wood", -1) end, "closing_speed")
fails("damage nan", function() Rules.impact_damage("wood", 0 / 0) end, "closing_speed")
check("target threshold", Rules.target_damage(1.5) == 0 and near(Rules.target_damage(2.5), 20))
fails("target speed", function() Rules.target_damage(math.huge) end, "closing_speed")

-- Stars ---------------------------------------------------------------------------
local th = {1000, 3000, 6000}
check("lost no stars", Rules.stars(99999, th, false) == 0)
check("win min one", Rules.stars(0, th, true) == 1)
check("one star", Rules.stars(1000, th, true) == 1)
check("below two", Rules.stars(2999, th, true) == 1)
check("two stars", Rules.stars(3000, th, true) == 2)
check("below three", Rules.stars(5999.5, th, true) == 2)
check("three stars", Rules.stars(6000, th, true) == 3)
fails("stars order", function() Rules.stars(1, {3, 2, 1}, true) end, "ascending")
fails("stars equal", function() Rules.stars(1, {1, 1, 2}, true) end, "ascending")
fails("stars count", function() Rules.stars(1, {1, 2}, true) end, "three")
fails("stars won", function() Rules.stars(1, th, 1) end, "won")
fails("stars score", function() Rules.stars(-1, th, true) end, "score")

-- Level validation ---------------------------------------------------------------
local function level()
  return {
    name = "Test Hill", shots = 3, stars = {1000, 3000, 6000},
    targets = {{id = "grump_a"}, {id = "grump_b"}},
    blocks = {{id = "plank", material = "wood"}, {id = "pane", material = "glass"},
              {id = "slab", material = "stone"}},
  }
end
check("valid level", Rules.validate_level(level()).shots == 3)
local function broken(change, pattern)
  local data = level()
  change(data)
  fails("level " .. pattern, function() Rules.validate_level(data) end, pattern)
end
fails("level type", function() Rules.validate_level(nil) end, "level must be a table")
broken(function(l) l.shots = 0 end, "level.shots")
broken(function(l) l.shots = 2.5 end, "integer")
broken(function(l) l.shots = 50 end, "level.shots")
broken(function(l) l.stars = {3000, 1000, 6000} end, "ascending")
broken(function(l) l.stars = {1, 2} end, "three")
broken(function(l) l.stars[2] = 0 / 0 end, "level.stars[2]")
broken(function(l) l.targets = {} end, "at least one target")
broken(function(l) l.targets = nil end, "at least one target")
broken(function(l) l.targets[2] = 7 end, "targets[2]")
broken(function(l) l.targets[1].id = "" end, "targets[1].id")
broken(function(l) l.blocks[1].material = "lava" end, "blocks[1].material")
broken(function(l) l.blocks[2].id = "grump_a" end, "duplicate id grump_a")
broken(function(l) l.blocks = "none" end, "level.blocks")
broken(function(l) l.name = 3 end, "level.name")
broken(function(l) l.kill_y = math.huge end, "level.kill_y")
fails("new_game validates", function() Rules.new_game({shots = 1}) end, "level.stars")

-- New materials: each has a distinct role.
check("all materials", M.ice and M.metal and M.rubber and M.brick and #Rules.MATERIAL_NAMES == 8)
check("ice fragile and slippery", M.ice.health < M.wood.health and M.ice.min_speed < M.wood.min_speed
  and M.ice.friction < 0.1 and M.ice.friction < M.glass.friction)
check("metal heaviest and toughest", M.metal.density > M.stone.density
  and M.metal.health > 4 * M.stone.health)
check("metal survives fast hits and blasts", Rules.impact_damage("metal", 20) < M.metal.health
  and Rules.config().blast_damage < M.metal.health)
check("rubber bouncy, light, tough", M.rubber.restitution > 0.6 and M.rubber.density < M.wood.density
  and Rules.impact_damage("rubber", 10) < M.rubber.health)
check("brick between wood and stone", M.brick.density > M.wood.density
  and M.brick.density < M.stone.density and M.brick.health > M.wood.health
  and M.brick.health < M.stone.health)
check("ice breaks at 2.5 m/s", Rules.impact_damage("ice", 2.5) >= M.ice.health)

-- Full level data: positions, sizes, spheres, and play fields.
local function full()
  return {
    name = "Full", shots = 2, stars = {1, 2, 3}, kill_y = -1, pouch = {0, 3.3, 0},
    aim = {yaw = 0, pitch = 16, power = 0.42}, fort_center = {0, 1, -18},
    ball = {radius = 0.3, density = 3500, friction = 0.5, restitution = 0.2}, target_radius = 0.45,
    targets = {{id = "g", position = {0, 0.45, -18}}},
    blocks = {{id = "box", material = "brick", position = {1, 0.5, -18}, half_extents = {0.5, 0.5, 0.5}},
              {id = "rock", material = "metal", shape = "sphere", position = {2, 0.4, -18}, radius = 0.4},
              {id = "board", material = "ice", shape = "box", position = {3, 0.1, -18},
               half_extents = {2, 0.1, 0.4}}},
  }
end
check("full level playable", Rules.check_playable(full()).shots == 2)
check("sphere mass", near(Rules.block_mass(full().blocks[2]), 4 / 3 * math.pi * 0.4 ^ 3 * 5000, 1e-6))
check("box mass", near(Rules.block_mass(full().blocks[1]), 1900))
local function bad_full(change, pattern)
  local data = full()
  change(data)
  fails("full level " .. pattern, function() Rules.check_playable(data) end, pattern)
end
local turned = full()
turned.blocks[1].rotation = {0, 1.5708, 0.01}
check("rotated block valid", Rules.check_playable(turned) == turned)
bad_full(function(l) l.blocks[1].rotation = {0, 9, 0} end, "rotation[2]")
bad_full(function(l) l.blocks[1].rotation = {0, 0 / 0, 0} end, "rotation")
bad_full(function(l) l.blocks[2].radius = nil end, "blocks[2].radius")
bad_full(function(l) l.blocks[2].radius = 9 end, "blocks[2].radius")
bad_full(function(l) l.blocks[2].half_extents = {1, 1, 1} end, "only for box")
bad_full(function(l) l.blocks[1].radius = 1 end, "only for sphere")
bad_full(function(l) l.blocks[1].shape = "cone" end, "box or sphere")
bad_full(function(l) l.blocks[1].half_extents = {0.5, 0, 0.5} end, "half_extents[2]")
bad_full(function(l) l.blocks[1].half_extents = {0.5, 7, 0.5} end, "half_extents[2]")
bad_full(function(l) l.blocks[1].half_extents = nil end, "half_extents is required")
bad_full(function(l) l.blocks[1].position = {1, 0 / 0, 2} end, "position")
bad_full(function(l) l.blocks[1].position = {1, 2, 500} end, "position[3]")
bad_full(function(l) l.blocks[1].position = nil end, "position is required")
bad_full(function(l) l.blocks[1].colour = "red" end, "unknown field colour")
bad_full(function(l) l.targets[1].position = nil end, "targets[1].position is required")
bad_full(function(l) l.targets[1].radius = 1 end, "unknown field radius")
bad_full(function(l) l.title = "x" end, "unknown field title")
bad_full(function(l) l.name = "" end, "level.name")
bad_full(function(l) l.name = string.rep("x", 49) end, "level.name")
bad_full(function(l) l.pouch = nil end, "level.pouch is required")
bad_full(function(l) l.pouch = {0, 1} end, "level.pouch")
bad_full(function(l) l.fort_center = "here" end, "level.fort_center")
bad_full(function(l) l.aim.pitch = 80 end, "aim.pitch")
bad_full(function(l) l.aim.roll = 1 end, "unknown field roll")
bad_full(function(l) l.ball.radius = 0 end, "level.ball.radius")
bad_full(function(l) l.ball.restitution = 2 end, "level.ball.restitution")
bad_full(function(l) l.ball.spin = 1 end, "unknown field spin")
bad_full(function(l) l.target_radius = 5 end, "level.target_radius")
bad_full(function(l) l.blocks[5] = {id = "gap", material = "wood"} end, "without gaps")
bad_full(function(l) l.blocks.extra = {} end, "without gaps")
bad_full(function(l) for i = 1, 401 do l.blocks[i] = {id = "b" .. i, material = "wood"} end end,
         "more than 400")
bad_full(function(l) l.blocks[1].material = "lava" end, "brick, glass, ice, metal, rubber, stone, tnt, wood")

-- Shot flow ----------------------------------------------------------------------
local fast = Rules.config({settle_ticks = 3, settle_timeout = 2, flight_timeout = 1})
local dt = 1 / 64 -- exact in binary, so timeouts land on a known tick
local calm = {0.01, 0}
local function settle(game)
  for _ = 1, 3 do game:tick(dt, calm) end
  return game.state
end

-- Miss, then lose when shots run out with grumps standing.
local game = Rules.new_game(level(), fast)
check("starts aiming", game.state == "aiming" and game.shots_left == 3 and game.score == 0)
fails("tick speeds validated", function() game:tick(dt, {-1}) end, "speeds[1]")
local vel = game:launch(rest)
check("launch velocity", near(vel[3], Rules.launch_velocity(rest, fast)[3]))
check("flying", game.state == "flying" and game.shots_left == 2)
fails("double launch", function() game:launch(rest) end, "cannot launch while flying")
check("fast bodies keep flying", game:tick(dt, {20}) == "flying")
check("landed", game:landed() == "settling")
check("landed idempotent", game:landed() == "settling")
game:tick(dt, calm); game:tick(dt, calm)
check("one calm tick short", game:tick(dt, {1}) == "settling" and game.calm_ticks == 0)
check("calm resets then settles", settle(game) == "aiming" and game.calm_ticks == 0)
game:launch(rest); game:landed(); settle(game)
check("second miss", game.state == "aiming" and game.shots_left == 1)
game:launch(rest); game:landed()
check("last shot lost", settle(game) == "lost" and game.stars == 0 and game.shots_left == 0)
fails("launch after loss", function() game:launch(rest) end, "cannot launch while lost")
check("hits ignored when over", select(1, game:hit_block("plank", 20)) == 0)
check("ticks ignored when over", game:tick(dt, calm) == "lost")

-- Flight timeout and settle timeout.
game = Rules.new_game(level(), fast)
game:launch(rest)
for _ = 1, 63 do game:tick(dt, {30}) end
check("still flying before timeout", game.state == "flying")
game:tick(dt, {30})
check("flight timeout", game.state == "settling")
for _ = 1, 127 do game:tick(dt, {5}) end
check("not settled before timeout", game.state == "settling")
check("settle timeout", game:tick(dt, {5}) == "aiming")

-- Damage on blocks and targets, win with shots to spare.
game = Rules.new_game(level(), fast)
game:launch(rest)
local damage, broke = game:hit_block("pane", 0.5)
check("soft glass hit", damage == 0 and not broke and game.blocks.pane.health == 30)
damage, broke = game:hit_block("pane", 3)
check("glass shatters", near(damage, 40) and broke and game.score == 50)
check("broken block ignored", select(1, game:hit_block("pane", 20)) == 0 and game.score == 50)
damage, broke = game:hit_block("slab", 10)
check("stone chipped", near(damage, 130) and not broke and near(game.blocks.slab.health, 30))
damage, broke = game:hit_block("slab", 5.1)
check("stone breaks", broke and game.blocks.slab.health == 0 and game.score == 450
  and game.combo == 2 and game.last_points == 400)
damage, broke = game:hit_target("grump_a", 1.5)
check("target soft hit", damage == 0 and not broke)
damage, broke = game:hit_target("grump_a", 2.5)
check("target bruised", near(damage, 20) and not broke and game.targets_left == 2)
damage, broke = game:hit_target("grump_a", 4)
check("target defeated by damage", broke and game.targets_left == 1 and game.score == 1950)
check("target above kill height", not game:target_height("grump_b", -1.9))
check("target defeated by falling", game:target_height("grump_b", -2.1)
  and game.targets_left == 0 and game.score == 3950)
check("fallen target ignored", not game:target_height("grump_b", -9))
check("win waits for settle", game.state == "flying")
game:landed()
check("won", settle(game) == "won" and game.bonus == 2000 and game.score == 5950
  and game.stars == 2 and game.best_combo == 4)
fails("launch after win", function() game:launch(rest) end, "cannot launch while won")

-- The last shot wins: no bonus, at least one star.
game = Rules.new_game({shots = 1, stars = {500, 2000, 4000}, targets = {{id = "g"}}, blocks = {},
                       kill_y = 1}, fast)
game:launch(rest)
check("level kill_y", not game:target_height("g", 1) and game:target_height("g", 0.99))
check("last shot not lost", game.shots_left == 0 and game.state == "flying")
game:landed()
check("last shot wins", settle(game) == "won" and game.bonus == 0 and game.score == 500
  and game.stars == 1)

-- A grump toppling between shots settles the yard, then wins.
game = Rules.new_game(level(), fast)
game:target_height("grump_a", -5)
check("defeat while aiming settles", game.state == "settling")
check("partial defeat back to aiming", settle(game) == "aiming")
game:hit_target("grump_b", 50)
check("between-shot win", game.state == "settling" and settle(game) == "won" and game.bonus == 3000
  and game.stars == 2)

-- Combos: each break in one shot raises the multiplier up to combo_max;
-- a new shot starts again at 1x.
check("combo multiplier", Rules.combo_multiplier(1) == 1 and Rules.combo_multiplier(3) == 3
  and Rules.combo_multiplier(9) == 5 and Rules.combo_multiplier(9, Rules.config({combo_max = 2})) == 2)
fails("combo count", function() Rules.combo_multiplier(0) end, "count")
fails("combo integer", function() Rules.combo_multiplier(1.5) end, "integer")
fails("combo config", function() Rules.config({combo_max = 0}) end, "combo_max")
local many = {shots = 3, stars = {1, 2, 3}, targets = {{id = "g"}}, blocks = {}}
for i = 1, 7 do many.blocks[i] = {id = "b" .. i, material = "glass"} end
game = Rules.new_game(many, fast)
game:launch(rest)
for i = 1, 7 do game:hit_block("b" .. i, 10) end
check("combo capped", game.combo == 7 and game.score == 50 * (1 + 2 + 3 + 4 + 5 + 5 + 5))
game:landed(); settle(game)
game:launch(rest)
check("combo resets per shot", game.combo == 0 and game.best_combo == 7)

-- Explosions: linear falloff, damage without a speed threshold, chains.
check("blast center", Rules.blast_falloff(0) == 1)
local R, D = cfg.blast_radius, cfg.blast_damage
check("blast half", near(Rules.blast_falloff(R / 2), 0.5))
check("blast edge", Rules.blast_falloff(R) == 0 and Rules.blast_falloff(50) == 0)
check("blast radius config", near(Rules.blast_falloff(1, Rules.config({blast_radius = 2})), 0.5))
fails("blast distance", function() Rules.blast_falloff(-1) end, "distance")
fails("blast nan", function() Rules.blast_falloff(0 / 0) end, "distance")
fails("blast radius", function() Rules.config({blast_radius = 0}) end, "blast_radius")
check("tnt fragile", M.tnt.explosive and M.tnt.health < M.glass.health + 1)
local crates = {shots = 2, stars = {1, 2, 3}, targets = {{id = "g"}},
                blocks = {{id = "crate", material = "tnt"}, {id = "crate2", material = "tnt"},
                          {id = "wall", material = "stone"}}}
game = Rules.new_game(Rules.validate_level(crates), fast)
check("explosive lookup", game:explosive("crate") and not game:explosive("wall")
  and not game:explosive("g") and not game:explosive("nope"))
game:launch(rest)
damage, broke = game:hit_block("crate", 4)
check("crate breaks", broke and near(damage, 40))
damage, broke = game:blast("crate2", 3.1)
check("faint blast spares a crate", not broke and near(damage, D * (1 - 3.1 / R)))
damage, broke = game:blast("crate2", 1.0)
check("chain crate", broke)
damage, broke = game:blast("wall", 0.5)
check("stone survives blast", not broke and near(game.blocks.wall.health, 160 - D * (1 - 0.5 / R)))
damage, broke = game:blast("g", R - 0.1)
check("far blast bruises", not broke and damage > 0 and damage < 4)
damage, broke = game:blast("g", 1)
check("blast defeats grump", broke and game.targets_left == 0)
check("blasted again ignored", select(1, game:blast("g", 0)) == 0)
check("out of reach", select(1, game:blast("wall", 9)) == 0)
fails("blast unknown", function() game:blast("zz", 1) end, "unknown body")
fails("blast negative", function() game:blast("wall", -1) end, "distance")

-- Invalid shot-flow calls.
game = Rules.new_game(level(), fast)
fails("launch aim", function() game:launch({yaw = 0, pitch = 1, power = 0.5}) end, "aim.pitch")
check("failed launch keeps shot", game.shots_left == 3 and game.state == "aiming")
fails("unknown block", function() game:hit_block("nope", 3) end, "unknown block nope")
fails("unknown target", function() game:hit_target("plank", 3) end, "unknown target plank")
fails("block speed", function() game:hit_block("plank", -2) end, "closing_speed")
fails("target speed", function() game:hit_target("grump_a", 0 / 0) end, "closing_speed")
fails("target height", function() game:target_height("grump_a", math.huge) end, "y")
fails("unknown height target", function() game:target_height("zz", 0) end, "unknown target")
fails("tick dt zero", function() game:tick(0, {}) end, "dt")
fails("tick dt large", function() game:tick(1, {}) end, "dt")
fails("tick speeds type", function() game:tick(dt, 3) end, "speeds")
fails("tick speeds nan", function() game:tick(dt, {0 / 0}) end, "speeds[1]")
fails("method without game", function() game.launch({}, rest) end, "Rules.new_game")
check("aiming tick no-op", game:tick(dt, {}) == "aiming")

print(string.format("siege_sling_rules: %d checks passed", passed))
