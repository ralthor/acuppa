-- Siege Sling level editor logic with no engine calls: grid snapping, stack
-- placement, block turning, a stability tracker, undo history, star
-- suggestions, generated names, and level building. scripts/editor.lua does
-- the engine work; tests/siege_sling_play_modules.lua tests this module.
local Core = {}

Core.GRID = 0.25
Core.CALM_SPEED = 0.15    -- m/s; every body must stay slower to count as calm
Core.CALM_SECONDS = 1.5   -- calm time before the layout is STABLE

local function finite(v) return type(v) == "number" and v == v and v > -math.huge and v < math.huge end

local function check_vec(where, name, v)
  if type(v) ~= "table" or not (finite(v[1]) and finite(v[2]) and finite(v[3])) then
    error(where .. ": " .. name .. " must hold three finite numbers", 3)
  end
end

-- Rounds to the nearest grid step (default 0.25 m).
function Core.snap(value, step)
  step = step or Core.GRID
  if not finite(value) or not finite(step) or step <= 0 then
    error("Core.snap: value and a positive step must be finite numbers", 2)
  end
  return math.floor(value / step + 0.5) * step
end

-- Half extents of an item: boxes use their own, spheres {r, r, r}.
function Core.half(item)
  if item.shape == "sphere" then return {item.radius, item.radius, item.radius} end
  check_vec("Core.half", "half_extents", item.half_extents)
  return item.half_extents
end

-- Turned 90 degrees about Y: X and Z swap.
function Core.rotate_y(h)
  check_vec("Core.rotate_y", "half extents", h)
  return {h[3], h[2], h[1]}
end

-- Tipped over: the vertical axis swaps with the longer horizontal axis.
function Core.tip(h)
  check_vec("Core.tip", "half extents", h)
  if h[1] >= h[3] then return {h[2], h[1], h[3]} end
  return {h[1], h[3], h[2]}
end

-- Highest point of a box (Euler rotation {x, y, z} in radians, applied Y,
-- then X, then Z, as in transforms) or sphere centered at position.
function Core.top(position, half, rotation, sphere)
  check_vec("Core.top", "position", position)
  check_vec("Core.top", "half", half)
  if sphere then return position[2] + half[2] end
  local r = rotation or {0, 0, 0}
  check_vec("Core.top", "rotation", r)
  local cx, sx = math.cos(r[1]), math.sin(r[1])
  local cz, sz = math.cos(r[3]), math.sin(r[3])
  return position[2] + math.abs(cx * sz) * half[1] + math.abs(cx * cz) * half[2] +
    math.abs(sx) * half[3]
end

-- Where an item with half extents `half` goes for a ray hit at `point`.
-- On the ground (support == nil) it rests on the hit height; on a support
-- ({top = y}) it rests on that top face. X and Z snap to the grid.
function Core.place(point, half, support)
  check_vec("Core.place", "point", point)
  check_vec("Core.place", "half", half)
  local base = point[2]
  if support ~= nil then
    if type(support) ~= "table" or not finite(support.top) then
      error("Core.place: support must be {top = number}", 2)
    end
    base = support.top
  end
  return {Core.snap(point[1]), base + half[2], Core.snap(point[3])}
end

-- Stability: STABLE once every speed stays under CALM_SPEED for CALM_SECONDS.
local Tracker = {}
Tracker.__index = Tracker

function Core.tracker(options)
  options = options or {}
  local self = setmetatable({speed = options.speed or Core.CALM_SPEED,
                             seconds = options.seconds or Core.CALM_SECONDS,
                             calm = 0, moving = 0, stable = false}, Tracker)
  if not finite(self.speed) or self.speed <= 0 or not finite(self.seconds) or self.seconds <= 0 then
    error("Core.tracker: speed and seconds must be positive", 2)
  end
  return self
end

-- Feeds one step of body speeds; returns stable, moving count.
function Tracker:update(dt, speeds)
  if not finite(dt) or dt < 0 or dt > 0.25 then error("tracker:update: dt must be in [0, 0.25]", 2) end
  if type(speeds) ~= "table" then error("tracker:update: speeds must be an array", 2) end
  local moving = 0
  for i = 1, #speeds do
    if not finite(speeds[i]) then error("tracker:update: speeds must be finite", 2) end
    if speeds[i] >= self.speed then moving = moving + 1 end
  end
  self.moving = moving
  if moving > 0 then self.calm = 0 else self.calm = self.calm + dt end
  self.stable = self.calm >= self.seconds - 1e-9
  return self.stable, moving
end

-- Forgets calm time (after an edit).
function Tracker:reset() self.calm, self.stable = 0, false end

-- Undo history of layouts, newest last, capped.
local History = {}
History.__index = History

function Core.history(limit)
  limit = limit or 50
  if math.type(limit) ~= "integer" or limit < 1 then error("Core.history: limit must be a positive integer", 2) end
  return setmetatable({limit = limit, items = {}}, History)
end

local function copy(value)
  if type(value) ~= "table" then return value end
  local result = {}
  for k, v in pairs(value) do result[k] = copy(v) end
  return result
end
Core.copy = copy

function History:push(layout)
  if type(layout) ~= "table" then error("history:push: layout must be a table", 2) end
  self.items[#self.items + 1] = copy(layout)
  while #self.items > self.limit do table.remove(self.items, 1) end
end

function History:pop() return table.remove(self.items) end
function History:count() return #self.items end
function History:clear() self.items = {} end

-- Suggested star scores from what can be scored: breaking blocks (by
-- material points), grumps, and the bonus for spare shots.
function Core.suggest_stars(blocks, grumps, shots, materials, grump_points, shot_bonus)
  if type(blocks) ~= "table" or type(materials) ~= "table" then
    error("Core.suggest_stars: blocks and materials must be tables", 2)
  end
  if math.type(grumps) ~= "integer" or grumps < 0 or math.type(shots) ~= "integer" or shots < 1 then
    error("Core.suggest_stars: grumps and shots must be integers (shots >= 1)", 2)
  end
  local block_points = 0
  for _, block in ipairs(blocks) do
    local m = materials[block.material]
    if not m then error("Core.suggest_stars: unknown material " .. tostring(block.material), 2) end
    block_points = block_points + m.points
  end
  local base = grumps * (grump_points or 500)
  local bonus = shot_bonus or 1000
  local function round(v) return math.floor(v / 500 + 0.5) * 500 end
  local one = math.max(500, round(base + 0.3 * block_points))
  local two = math.max(one + 500, round(base + 0.7 * block_points + bonus))
  local three = math.max(two + 500, round(base + 1.2 * block_points + bonus * math.max(1, shots - 2)))
  return {one, two, three}
end

-- Fun generated level names (there is no text input).
Core.NAMES = {"Pebble Keep", "Wobble Tower", "Crate Canyon", "Frosty Fort", "Rubber Ramparts",
              "Brick Bastion", "Glass Garden", "Iron Hollow", "Teetering Tops", "Grump Gulch",
              "Boulder Bluff", "Splinter Spire"}

-- Display name for name index i: generated names, then "Custom Level N".
function Core.name(i)
  if math.type(i) ~= "integer" or i < 1 then error("Core.name: index must be a positive integer", 2) end
  return Core.NAMES[i] or ("Custom Level " .. (i - #Core.NAMES))
end

-- File-safe slug for a level name, never "index".
function Core.slug(name)
  if type(name) ~= "string" or name == "" then error("Core.slug: name must be a nonempty string", 2) end
  local slug = name:lower():gsub("[^%w]+", "_"):gsub("^_+", ""):gsub("_+$", "")
  if slug == "" then slug = "level" end
  if slug == "index" then slug = "index_level" end
  return slug:sub(1, 40)
end

-- Next unused id with a prefix: prefix_1, prefix_2, ...
function Core.unique_id(prefix, used)
  local i = 1
  while used[prefix .. "_" .. i] do i = i + 1 end
  return prefix .. "_" .. i
end

-- Rounds to a number of decimals, giving the double nearest that decimal.
local function round(v, digits)
  local r = tonumber(string.format("%." .. digits .. "f", v))
  if r == 0 then r = 0.0 end
  return r
end

-- Rounds a settled pose for a clean file: millimeters, 1e-4 rad, and
-- rotations that are nearly zero dropped. Returns position, rotation|nil.
function Core.clean_pose(position, rotation)
  check_vec("Core.clean_pose", "position", position)
  local p = {round(position[1], 3), round(position[2], 3), round(position[3], 3)}
  if rotation == nil then return p, nil end
  check_vec("Core.clean_pose", "rotation", rotation)
  local r, any = {}, false
  for i = 1, 3 do
    r[i] = math.abs(rotation[i]) < 0.002 and 0.0 or round(rotation[i], 4)
    any = any or r[i] ~= 0
  end
  return p, any and r or nil
end

-- Builds level data from layout items ({kind = "block"|"target", ...} with
-- positions), settings ({name, shots, stars}), and base play fields.
function Core.build_level(items, settings, base)
  local level = {name = settings.name, shots = settings.shots,
                 stars = {settings.stars[1], settings.stars[2], settings.stars[3]},
                 kill_y = base.kill_y or -1, pouch = copy(base.pouch or {0, 3.3, 0}),
                 aim = copy(base.aim or {yaw = 0, pitch = 16, power = 0.42}),
                 ball = copy(base.ball or {radius = 0.3, density = 3500, friction = 0.5, restitution = 0.2}),
                 target_radius = base.target_radius or 0.45, blocks = {}, targets = {}}
  local low, high = {math.huge, math.huge, math.huge}, {-math.huge, -math.huge, -math.huge}
  for _, item in ipairs(items) do
    local position, rotation = Core.clean_pose(item.position, item.rotation)
    for i = 1, 3 do
      low[i], high[i] = math.min(low[i], position[i]), math.max(high[i], position[i])
    end
    if item.kind == "target" then
      level.targets[#level.targets + 1] = {id = item.id, position = position}
    else
      local block = {id = item.id, material = item.material, position = position, rotation = rotation}
      if item.shape == "sphere" then
        block.shape, block.radius = "sphere", item.radius
      else
        block.half_extents = copy(item.half_extents)
      end
      level.blocks[#level.blocks + 1] = block
    end
  end
  if low[1] == math.huge then
    level.fort_center = {0, 1.5, -18}
  else
    level.fort_center = {round((low[1] + high[1]) / 2, 2), round(math.max(1, (low[2] + high[2]) / 2), 2),
                         round((low[3] + high[3]) / 2, 2)}
  end
  return level
end

return Core
