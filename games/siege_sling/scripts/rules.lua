-- Siege Sling rules: aim, launch, arc prediction, damage, shot flow, score.
-- Pure Lua with no engine calls, randomness, or globals, so every rule can be
-- tested without a window.
--
-- Units and axes: meters, seconds, meters/second, degrees for yaw and pitch.
-- Vectors are {x, y, z} arrays with Y up. The sling fires along -Z when yaw is
-- 0 (the forts stand down -Z). Positive yaw turns the shot right, toward +X;
-- positive pitch raises it. Screen drags use window pixels with y growing
-- downward, so pulling the pouch down and left launches the ball up and right.
local Rules = {}

local DEFAULTS = {
  yaw_limit = 45,            -- degrees either side of -Z
  pitch_min = 5,             -- degrees above horizontal
  pitch_max = 70,
  max_drag = 240,            -- pixels for full power and full deflection
  cancel_drag = 18,          -- shorter drags cancel instead of firing
  yaw_rate = 60,             -- degrees/second for keys or a stick
  pitch_rate = 45,
  power_rate = 0.8,          -- power/second
  min_speed = 8,             -- launch speed at power 0 (m/s)
  max_speed = 28,            -- launch speed at power 1 (m/s)
  gravity = -9.81,           -- m/s^2 along Y
  damage_per_speed = 20,     -- damage per m/s above a material's threshold
  target_health = 40,
  target_min_speed = 1.5,    -- grumps ignore softer contacts
  target_points = 500,
  kill_y = -2,               -- grumps below this height are defeated
  shot_bonus = 1000,         -- points per unused shot on a win
  combo_max = 5,             -- the Nth break in one shot scores min(N, combo_max)x
  blast_radius = 3.2,        -- explosive crate reach (m)
  blast_speed = 6,           -- velocity kick at the crate's center (m/s)
  blast_damage = 90,         -- damage at the crate's center
  flight_timeout = 6,        -- seconds before a shot that never lands settles
  settle_speed = 0.15,       -- m/s; every tracked body must be slower
  settle_ticks = 30,         -- consecutive calm fixed ticks (0.5 s at 60 Hz)
  settle_timeout = 8,        -- seconds of settling before forcing a result
}

-- Physics values feed the scene's physics component (Phase 28 fields).
-- Glass breaks from gentle knocks, wood takes a solid hit, stone needs a
-- near-direct fast shot or a heavy fall.
Rules.MATERIALS = {
  wood = {health = 60, min_speed = 2.0, density = 600, friction = 0.6,
          restitution = 0.15, points = 100},
  stone = {health = 160, min_speed = 3.5, density = 2400, friction = 0.8,
           restitution = 0.05, points = 200},
  glass = {health = 30, min_speed = 1.0, density = 1500, friction = 0.25,
           restitution = 0.3, points = 50},
  -- Explosive crate: breaks easily and blasts its surroundings.
  tnt = {health = 20, min_speed = 2.0, density = 500, friction = 0.6,
         restitution = 0.1, points = 150, explosive = true},
}

local function fail(where, message) error(where .. ": " .. message, 3) end

local function finite(value)
  return type(value) == "number" and value == value and value > -math.huge
    and value < math.huge
end

local function check_number(where, name, value, low, high)
  if not finite(value) then fail(where, name .. " must be a finite number") end
  if (low and value < low) or (high and value > high) then
    fail(where, string.format("%s must be in [%s, %s]", name,
      low and tostring(low) or "-inf", high and tostring(high) or "inf"))
  end
  return value
end

local function check_integer(where, name, value, low, high)
  check_number(where, name, value, low, high)
  if math.type(value) ~= "integer" and value ~= math.floor(value) then
    fail(where, name .. " must be an integer")
  end
  return math.tointeger(value)
end

local function check_vec3(where, name, value)
  if type(value) ~= "table" then fail(where, name .. " must be an {x, y, z} table") end
  for i = 1, 3 do
    if not finite(value[i]) then
      fail(where, name .. " must hold three finite numbers")
    end
  end
  return value
end

local function clamp(value, low, high)
  if value < low then return low end
  if value > high then return high end
  return value
end

local function check_config(where, config)
  if config == nil then return DEFAULTS end
  if type(config) ~= "table" then fail(where, "config must be a table from Rules.config") end
  for key in pairs(config) do
    if DEFAULTS[key] == nil then fail(where, "unknown config field " .. tostring(key)) end
  end
  for key in pairs(DEFAULTS) do
    if not finite(config[key]) then fail(where, "config." .. key .. " must be a finite number") end
  end
  local c = config
  if c.yaw_limit < 0 or c.yaw_limit > 90 then fail(where, "config.yaw_limit must be in [0, 90]") end
  if c.pitch_min < -45 or c.pitch_max > 89 or c.pitch_min > c.pitch_max then
    fail(where, "config pitch range must satisfy -45 <= pitch_min <= pitch_max <= 89")
  end
  if c.max_drag <= 0 or c.cancel_drag < 0 or c.cancel_drag >= c.max_drag then
    fail(where, "config drag lengths must satisfy 0 <= cancel_drag < max_drag")
  end
  for _, key in ipairs({"yaw_rate", "pitch_rate", "power_rate", "damage_per_speed",
                        "target_min_speed", "target_points", "shot_bonus", "settle_speed",
                        "blast_speed", "blast_damage"}) do
    if c[key] < 0 then fail(where, "config." .. key .. " must not be negative") end
  end
  if c.min_speed <= 0 or c.max_speed < c.min_speed then
    fail(where, "config speeds must satisfy 0 < min_speed <= max_speed")
  end
  if c.gravity >= 0 then fail(where, "config.gravity must be negative") end
  if c.blast_radius <= 0 then fail(where, "config.blast_radius must be positive") end
  if c.combo_max < 1 or c.combo_max ~= math.floor(c.combo_max) then
    fail(where, "config.combo_max must be a positive integer")
  end
  if c.target_health <= 0 then fail(where, "config.target_health must be positive") end
  if c.flight_timeout <= 0 or c.settle_timeout <= 0 then
    fail(where, "config timeouts must be positive")
  end
  if c.settle_ticks < 1 or c.settle_ticks ~= math.floor(c.settle_ticks) then
    fail(where, "config.settle_ticks must be a positive integer")
  end
  return config
end

-- Returns a new validated config: the defaults with `overrides` applied.
function Rules.config(overrides)
  if overrides ~= nil and type(overrides) ~= "table" then
    fail("Rules.config", "overrides must be a table or nil")
  end
  local config = {}
  for key, value in pairs(DEFAULTS) do config[key] = value end
  for key, value in pairs(overrides or {}) do
    if DEFAULTS[key] == nil then fail("Rules.config", "unknown config field " .. tostring(key)) end
    config[key] = value
  end
  return check_config("Rules.config", config)
end

local function check_aim(where, aim, config)
  if type(aim) ~= "table" then fail(where, "aim must be a {yaw, pitch, power} table") end
  check_number(where, "aim.yaw", aim.yaw, -config.yaw_limit, config.yaw_limit)
  check_number(where, "aim.pitch", aim.pitch, config.pitch_min, config.pitch_max)
  check_number(where, "aim.power", aim.power, 0, 1)
  return aim
end

-- Aim at rest: straight down -Z, halfway up the pitch range, half power.
function Rules.default_aim(config)
  config = check_config("Rules.default_aim", config)
  return {yaw = 0, pitch = (config.pitch_min + config.pitch_max) / 2, power = 0.5}
end

-- Maps a pull from the sling (pixels, y down) to an aim. Horizontal pull sets
-- yaw opposite the pull, downward pull raises pitch from pitch_min to
-- pitch_max over max_drag, and pull length sets power. Pulls shorter than
-- cancel_drag return nil, "cancel".
function Rules.aim_from_drag(dx, dy, config)
  local where = "Rules.aim_from_drag"
  check_number(where, "dx", dx)
  check_number(where, "dy", dy)
  config = check_config(where, config)
  local length = math.sqrt(dx * dx + dy * dy)
  if length < config.cancel_drag then return nil, "cancel" end
  local across = clamp(-dx / config.max_drag, -1, 1)
  local down = clamp(dy / config.max_drag, 0, 1)
  return {
    yaw = across * config.yaw_limit,
    pitch = config.pitch_min + down * (config.pitch_max - config.pitch_min),
    power = clamp(length / config.max_drag, 0, 1),
  }
end

-- Moves an aim by key or stick axes in [-1, 1] ({yaw, pitch, power}, each
-- optional) at the configured rates for dt seconds. Returns a new clamped aim.
function Rules.adjust_aim(aim, input, dt, config)
  local where = "Rules.adjust_aim"
  config = check_config(where, config)
  check_aim(where, aim, config)
  if type(input) ~= "table" then fail(where, "input must be a {yaw, pitch, power} table") end
  for key in pairs(input) do
    if key ~= "yaw" and key ~= "pitch" and key ~= "power" then
      fail(where, "unknown input axis " .. tostring(key))
    end
  end
  local yaw = check_number(where, "input.yaw", input.yaw or 0, -1, 1)
  local pitch = check_number(where, "input.pitch", input.pitch or 0, -1, 1)
  local power = check_number(where, "input.power", input.power or 0, -1, 1)
  check_number(where, "dt", dt, 0, 0.25)
  return {
    yaw = clamp(aim.yaw + yaw * config.yaw_rate * dt, -config.yaw_limit, config.yaw_limit),
    pitch = clamp(aim.pitch + pitch * config.pitch_rate * dt, config.pitch_min, config.pitch_max),
    power = clamp(aim.power + power * config.power_rate * dt, 0, 1),
  }
end

-- Launch speed is min_speed + power * (max_speed - min_speed); direction is
-- {cos(p) sin(y), sin(p), -cos(p) cos(y)}.
function Rules.launch_velocity(aim, config)
  local where = "Rules.launch_velocity"
  config = check_config(where, config)
  check_aim(where, aim, config)
  local speed = config.min_speed + aim.power * (config.max_speed - config.min_speed)
  local yaw, pitch = math.rad(aim.yaw), math.rad(aim.pitch)
  return {speed * math.cos(pitch) * math.sin(yaw), speed * math.sin(pitch),
          -speed * math.cos(pitch) * math.cos(yaw)}
end

-- Samples the drag-free ballistic arc p(t) = origin + v t + g t^2 / 2 every
-- `step` seconds. Options: step (default 1/30), max_points (default 120),
-- ground_y (default 0; false disables the ground stop), gravity (default
-- config gravity). Stops at max_points or at the exact ground crossing, which
-- becomes the last point and the second result. The game launches the ball
-- with linear_damping = 0, so only Jolt's semi-implicit Euler integration
-- differs: it lands about g * dt * t / 2 lower (about 8 cm per second of
-- flight at 60 Hz), which is small for an aim guide.
function Rules.predict_arc(origin, velocity, options, config)
  local where = "Rules.predict_arc"
  check_vec3(where, "origin", origin)
  check_vec3(where, "velocity", velocity)
  config = check_config(where, config)
  if options ~= nil and type(options) ~= "table" then fail(where, "options must be a table or nil") end
  options = options or {}
  for key in pairs(options) do
    if key ~= "step" and key ~= "max_points" and key ~= "ground_y" and key ~= "gravity" then
      fail(where, "unknown option " .. tostring(key))
    end
  end
  local step = check_number(where, "options.step", options.step or 1 / 30, 1e-4, 1)
  local max_points = check_integer(where, "options.max_points", options.max_points or 120, 2, 10000)
  local gravity = check_number(where, "options.gravity", options.gravity or config.gravity)
  local ground = options.ground_y
  if ground == nil then ground = 0 end
  if ground ~= false then
    check_number(where, "options.ground_y", ground)
    if origin[2] < ground then fail(where, "origin is below options.ground_y") end
  end
  local ox, oy, oz = origin[1], origin[2], origin[3]
  local vx, vy, vz = velocity[1], velocity[2], velocity[3]
  local points = {{ox, oy, oz}}
  for i = 1, max_points - 1 do
    local t = i * step
    local y = oy + vy * t + 0.5 * gravity * t * t
    if ground ~= false and y < ground then
      -- Later root of oy + vy t + g t^2 / 2 = ground (g < 0 gives a real root).
      local a, b, c = 0.5 * gravity, vy, oy - ground
      local hit
      if a == 0 then
        hit = -c / b
      else
        hit = (-b - math.sqrt(math.max(0, b * b - 4 * a * c))) / (2 * a)
      end
      local landing = {ox + vx * hit, ground, oz + vz * hit}
      points[#points + 1] = landing
      return points, landing
    end
    points[#points + 1] = {ox + vx * t, y, oz + vz * t}
  end
  return points, nil
end

local function check_material(where, name)
  if type(name) ~= "string" then fail(where, "material must be a string") end
  local material = Rules.MATERIALS[name]
  if not material then fail(where, "unknown material " .. name) end
  return material
end

-- Damage a block of `material` takes from a contact at closing_speed (m/s):
-- 0 at or below the material's min_speed, then damage_per_speed per m/s above.
function Rules.impact_damage(material, closing_speed, config)
  local where = "Rules.impact_damage"
  local data = check_material(where, material)
  check_number(where, "closing_speed", closing_speed, 0)
  config = check_config(where, config)
  if closing_speed <= data.min_speed then return 0 end
  return (closing_speed - data.min_speed) * config.damage_per_speed
end

-- Damage a grump takes from a contact; same rule with target_min_speed.
function Rules.target_damage(closing_speed, config)
  local where = "Rules.target_damage"
  check_number(where, "closing_speed", closing_speed, 0)
  config = check_config(where, config)
  if closing_speed <= config.target_min_speed then return 0 end
  return (closing_speed - config.target_min_speed) * config.damage_per_speed
end

-- Explosion strength at `distance` meters from a crate: 1 at the center,
-- falling linearly to 0 at blast_radius and beyond. Multiply blast_speed
-- (velocity kick) and blast_damage by it.
function Rules.blast_falloff(distance, config)
  local where = "Rules.blast_falloff"
  check_number(where, "distance", distance, 0)
  config = check_config(where, config)
  return math.max(0, 1 - distance / config.blast_radius)
end

-- Score multiplier for the Nth break within one shot.
function Rules.combo_multiplier(count, config)
  local where = "Rules.combo_multiplier"
  check_integer(where, "count", count, 1)
  config = check_config(where, config)
  return math.min(count, config.combo_max)
end

-- Stars for a final score: lost is 0; a win earns one star per threshold
-- reached, and always at least one.
function Rules.stars(score, thresholds, won)
  local where = "Rules.stars"
  check_number(where, "score", score, 0)
  if type(won) ~= "boolean" then fail(where, "won must be a boolean") end
  if type(thresholds) ~= "table" or #thresholds ~= 3 then
    fail(where, "thresholds must hold three numbers")
  end
  for i = 1, 3 do
    check_number(where, "thresholds[" .. i .. "]", thresholds[i], 0)
    if i > 1 and thresholds[i] <= thresholds[i - 1] then
      fail(where, "thresholds must be strictly ascending")
    end
  end
  if not won then return 0 end
  local stars = 0
  for i = 1, 3 do
    if score >= thresholds[i] then stars = i end
  end
  return math.max(1, stars)
end

local function check_id(where, name, value, seen)
  if type(value) ~= "string" or value == "" then fail(where, name .. " must be a nonempty string") end
  if seen[value] then fail(where, "duplicate id " .. value) end
  seen[value] = true
end

-- Validates a level table and returns it. Required: shots (integer 1..20),
-- stars (three strictly ascending scores >= 0), targets (nonempty array of
-- {id}), blocks (array of {id, material}). Optional: name (string), kill_y
-- (number). Ids are unique across blocks and targets. Other fields (such as
-- positions for the scene) are left to the scene.
function Rules.validate_level(level)
  local where = "Rules.validate_level"
  if type(level) ~= "table" then fail(where, "level must be a table") end
  if level.name ~= nil and type(level.name) ~= "string" then fail(where, "level.name must be a string") end
  check_integer(where, "level.shots", level.shots, 1, 20)
  if type(level.stars) ~= "table" or #level.stars ~= 3 then
    fail(where, "level.stars must hold three scores")
  end
  for i = 1, 3 do
    check_number(where, "level.stars[" .. i .. "]", level.stars[i], 0)
    if i > 1 and level.stars[i] <= level.stars[i - 1] then
      fail(where, "level.stars must be strictly ascending")
    end
  end
  if level.kill_y ~= nil then check_number(where, "level.kill_y", level.kill_y) end
  local seen = {}
  if type(level.targets) ~= "table" or #level.targets == 0 then
    fail(where, "level.targets must list at least one target")
  end
  for i, target in ipairs(level.targets) do
    if type(target) ~= "table" then fail(where, "level.targets[" .. i .. "] must be a table") end
    check_id(where, "level.targets[" .. i .. "].id", target.id, seen)
  end
  if type(level.blocks) ~= "table" then fail(where, "level.blocks must be an array") end
  for i, block in ipairs(level.blocks) do
    if type(block) ~= "table" then fail(where, "level.blocks[" .. i .. "] must be a table") end
    check_id(where, "level.blocks[" .. i .. "].id", block.id, seen)
    if type(block.material) ~= "string" or not Rules.MATERIALS[block.material] then
      fail(where, "level.blocks[" .. i .. "].material must be one of glass, stone, tnt, wood")
    end
  end
  return level
end

-- Shot flow ----------------------------------------------------------------
-- states: aiming -> flying -> settling -> aiming | won | lost
local Game = {}
Game.__index = Game

function Rules.new_game(level, config)
  local where = "Rules.new_game"
  Rules.validate_level(level)
  config = check_config(where, config)
  local game = setmetatable({
    config = config, state = "aiming", shots_left = math.tointeger(level.shots),
    score = 0, bonus = 0, stars = 0, thresholds = {level.stars[1], level.stars[2], level.stars[3]},
    kill_y = level.kill_y or config.kill_y, blocks = {}, targets = {},
    targets_left = #level.targets, timer = 0, calm_ticks = 0,
    combo = 0, best_combo = 0, last_points = 0,
  }, Game)
  for _, block in ipairs(level.blocks) do
    local material = Rules.MATERIALS[block.material]
    game.blocks[block.id] = {material = block.material, health = material.health, destroyed = false}
  end
  for _, target in ipairs(level.targets) do
    game.targets[target.id] = {health = config.target_health, defeated = false}
  end
  return game
end

local function check_game(where, game)
  if getmetatable(game) ~= Game then fail(where, "call with a game from Rules.new_game") end
end

function Game:finished() return self.state == "won" or self.state == "lost" end

-- Fires a shot from the aiming state; returns the launch velocity.
function Game:launch(aim)
  local where = "game:launch"
  check_game(where, self)
  if self.state ~= "aiming" then fail(where, "cannot launch while " .. self.state) end
  check_aim(where, aim, self.config)
  self.shots_left = self.shots_left - 1
  self.state, self.timer, self.calm_ticks = "flying", 0, 0
  self.combo = 0
  return Rules.launch_velocity(aim, self.config)
end

-- The ball touched something or left the yard: start waiting for calm.
function Game:landed()
  check_game("game:landed", self)
  if self.state == "flying" then
    self.state, self.timer, self.calm_ticks = "settling", 0, 0
  end
  return self.state
end

-- Each break within one shot raises the combo; points scale with it.
local function award(self, points)
  self.combo = self.combo + 1
  self.best_combo = math.max(self.best_combo, self.combo)
  local multiplier = Rules.combo_multiplier(self.combo, self.config)
  self.last_points = points * multiplier
  self.score = self.score + self.last_points
end

local function defeat(self, target)
  target.defeated = true
  self.targets_left = self.targets_left - 1
  award(self, self.config.target_points)
  -- A grump knocked over between shots still needs the yard to settle.
  if self.state == "aiming" then self.state, self.timer, self.calm_ticks = "settling", 0, 0 end
end

-- Applies a contact to a block; returns damage dealt and whether it broke now.
function Game:hit_block(id, closing_speed)
  local where = "game:hit_block"
  check_game(where, self)
  local block = self.blocks[id]
  if not block then fail(where, "unknown block " .. tostring(id)) end
  check_number(where, "closing_speed", closing_speed, 0)
  if block.destroyed or self:finished() then return 0, false end
  local damage = Rules.impact_damage(block.material, closing_speed, self.config)
  if damage <= 0 then return 0, false end
  block.health = block.health - damage
  if block.health > 0 then return damage, false end
  block.health, block.destroyed = 0, true
  award(self, Rules.MATERIALS[block.material].points)
  return damage, true
end

local function apply_damage(self, id, damage)
  local block, target = self.blocks[id], self.targets[id]
  if block then
    if block.destroyed or damage <= 0 then return 0, false end
    block.health = block.health - damage
    if block.health > 0 then return damage, false end
    block.health, block.destroyed = 0, true
    award(self, Rules.MATERIALS[block.material].points)
    return damage, true
  end
  if target.defeated or damage <= 0 then return 0, false end
  target.health = target.health - damage
  if target.health > 0 then return damage, false end
  target.health = 0
  defeat(self, target)
  return damage, true
end

-- Applies an explosion at `distance` meters to a block or grump (no speed
-- threshold); returns damage dealt and whether it broke now.
function Game:blast(id, distance)
  local where = "game:blast"
  check_game(where, self)
  if not self.blocks[id] and not self.targets[id] then fail(where, "unknown body " .. tostring(id)) end
  check_number(where, "distance", distance, 0)
  if self:finished() then return 0, false end
  return apply_damage(self, id, Rules.blast_falloff(distance, self.config) * self.config.blast_damage)
end

-- True when the named block explodes when it breaks.
function Game:explosive(id)
  check_game("game:explosive", self)
  local block = self.blocks[id]
  return block ~= nil and Rules.MATERIALS[block.material].explosive == true
end

-- Applies a contact to a grump; returns damage dealt and whether it fell now.
function Game:hit_target(id, closing_speed)
  local where = "game:hit_target"
  check_game(where, self)
  local target = self.targets[id]
  if not target then fail(where, "unknown target " .. tostring(id)) end
  check_number(where, "closing_speed", closing_speed, 0)
  if target.defeated or self:finished() then return 0, false end
  local damage = Rules.target_damage(closing_speed, self.config)
  if damage <= 0 then return 0, false end
  target.health = target.health - damage
  if target.health > 0 then return damage, false end
  target.health = 0
  defeat(self, target)
  return damage, true
end

-- Reports a grump's height; below kill_y defeats it. Returns true if now.
function Game:target_height(id, y)
  local where = "game:target_height"
  check_game(where, self)
  local target = self.targets[id]
  if not target then fail(where, "unknown target " .. tostring(id)) end
  check_number(where, "y", y)
  if target.defeated or self:finished() or y >= self.kill_y then return false end
  defeat(self, target)
  return true
end

local function resolve(self)
  if self.targets_left == 0 then
    self.state = "won"
    self.bonus = self.shots_left * self.config.shot_bonus
    self.score = self.score + self.bonus
    self.stars = Rules.stars(self.score, self.thresholds, true)
  elseif self.shots_left == 0 then
    self.state, self.stars = "lost", 0
  else
    self.state = "aiming"
  end
  self.timer, self.calm_ticks = 0, 0
end

-- Advances one fixed tick. `speeds` lists the current speeds (m/s) of every
-- tracked body (ball, blocks, grumps). Flying turns into settling after
-- flight_timeout; settling resolves after settle_ticks consecutive ticks with
-- every speed below settle_speed, or after settle_timeout. Returns the state.
function Game:tick(dt, speeds)
  local where = "game:tick"
  check_game(where, self)
  check_number(where, "dt", dt, 0, 0.25)
  if dt <= 0 then fail(where, "dt must be positive") end
  if type(speeds) ~= "table" then fail(where, "speeds must be an array of numbers") end
  local fastest = 0
  for i = 1, #speeds do
    local speed = check_number(where, "speeds[" .. i .. "]", speeds[i], 0)
    if speed > fastest then fastest = speed end
  end
  if self.state == "flying" then
    self.timer = self.timer + dt
    if self.timer >= self.config.flight_timeout then
      self.state, self.timer, self.calm_ticks = "settling", 0, 0
    end
  elseif self.state == "settling" then
    self.timer = self.timer + dt
    if fastest < self.config.settle_speed then
      self.calm_ticks = self.calm_ticks + 1
    else
      self.calm_ticks = 0
    end
    if self.calm_ticks >= self.config.settle_ticks or self.timer >= self.config.settle_timeout then
      resolve(self)
    end
  end
  return self.state
end

return Rules
