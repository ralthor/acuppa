-- Debris bursts for broken blocks and grumps: pure, deterministic motion.
-- Each piece flies out ballistically, bounces once or twice on the ground,
-- spins, and expires after `life` seconds. The total is capped; a burst that
-- would exceed the cap evicts the oldest pieces (reported for despawning).
local Debris = {}
Debris.__index = Debris

local GRAVITY = -9.81

local function finite(v)
  return type(v) == "number" and v == v and v > -math.huge and v < math.huge
end

function Debris.new(options)
  options = options or {}
  if type(options) ~= "table" then error("Debris.new: options must be a table", 2) end
  local self = setmetatable({cap = options.cap or 48, life = options.life or 1.0,
                             ground = options.ground or 0, pieces = {}, serial = 0}, Debris)
  if not finite(self.cap) or self.cap < 1 or self.cap ~= math.floor(self.cap) then
    error("Debris.new: cap must be a positive integer", 2)
  end
  if not finite(self.life) or self.life <= 0 then error("Debris.new: life must be positive", 2) end
  if not finite(self.ground) then error("Debris.new: ground must be finite", 2) end
  return self
end

-- Adds `count` pieces of `kind` at `center` (fan of directions, upward bias,
-- scaled by `speed`). Returns the new pieces and any evicted pieces.
function Debris:burst(kind, center, count, speed)
  if type(kind) ~= "string" then error("debris:burst: kind must be a string", 2) end
  if type(center) ~= "table" or not (finite(center[1]) and finite(center[2]) and finite(center[3])) then
    error("debris:burst: center must hold three finite numbers", 2)
  end
  if math.type(count) ~= "integer" or count < 1 or count > 32 then
    error("debris:burst: count must be an integer from 1 to 32", 2)
  end
  if not finite(speed) or speed <= 0 then error("debris:burst: speed must be positive", 2) end
  local added, evicted = {}, {}
  for i = 1, count do
    self.serial = self.serial + 1
    local angle = (i - 1) / count * 2 * math.pi + self.serial * 0.7
    local lift = 0.55 + 0.45 * ((i * 37) % 10) / 10
    local piece = {
      kind = kind, age = 0, serial = self.serial,
      position = {center[1], center[2], center[3]},
      velocity = {math.cos(angle) * speed, lift * speed * 1.2, math.sin(angle) * speed},
      spin = {0, 0, 0}, spin_rate = {7 + i, 5 + 2 * i, 3 + i},
    }
    self.pieces[#self.pieces + 1] = piece
    added[#added + 1] = piece
  end
  while #self.pieces > self.cap do
    evicted[#evicted + 1] = table.remove(self.pieces, 1)
  end
  return added, evicted
end

-- Advances every piece; returns the pieces that expired this step.
function Debris:step(dt)
  if not finite(dt) or dt < 0 or dt > 0.25 then error("debris:step: dt must be in [0, 0.25]", 2) end
  local expired, kept = {}, {}
  for _, p in ipairs(self.pieces) do
    p.age = p.age + dt
    if p.age >= self.life then
      expired[#expired + 1] = p
    else
      local v, x = p.velocity, p.position
      v[2] = v[2] + GRAVITY * dt
      for i = 1, 3 do
        x[i] = x[i] + v[i] * dt
        p.spin[i] = p.spin[i] + p.spin_rate[i] * dt
      end
      if x[2] < self.ground and v[2] < 0 then
        x[2] = self.ground
        v[1], v[2], v[3] = v[1] * 0.6, -v[2] * 0.35, v[3] * 0.6
      end
      kept[#kept + 1] = p
    end
  end
  self.pieces = kept
  return expired
end

function Debris:count() return #self.pieces end

return Debris
