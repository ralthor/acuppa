-- Deterministic replay effects: crash debris, the explosion flash, and wind
-- streaks. Replays are recordings, so these are animated in Lua instead of
-- being simulated, and the same replay always looks the same.
local Effects = {PIECES = 6, STREAKS = 24}

local GRAVITY = 9.81
local REST_Y = 0.12

-- Position and rotation of debris `piece` of lander `slot`, `t` seconds after
-- an explosion at `origin`. Pieces fly outward and stop where they hit the ground.
function Effects.debris(origin, slot, piece, t)
  local angle = piece / Effects.PIECES * 2 * math.pi + slot * 0.7
  local speed = 3.5 + piece % 3
  local vx, vz = math.cos(angle) * speed, math.sin(angle * 1.7) * 1.5
  local vy = math.abs(math.sin(angle)) * speed + 2.5
  local y0 = math.max(origin[2], REST_Y)
  -- Time until the piece falls back to resting height.
  local landing = (vy + math.sqrt(vy * vy + 2 * GRAVITY * (y0 - REST_Y))) / GRAVITY
  local s = math.min(math.max(t, 0), landing)
  local spin = (piece + 1) * 3 * s
  return {origin[1] + vx * s, math.max(REST_Y, y0 + vy * s - 0.5 * GRAVITY * s * s),
          origin[3] + vz * s}, {spin, spin * 0.6, spin * 0.3}
end

-- Size of the explosion flash `t` seconds after it starts; nil once it is over.
function Effects.flash(t)
  if t < 0 then return nil end
  if t < 0.12 then return 0.3 + t / 0.12 * 1.4 end
  if t < 0.5 then return 1.7 * (0.5 - t) / 0.38 end
  return nil
end

local function wrap(value, low, high)
  return low + (value - low) % (high - low)
end

-- Streak i drifts with the accumulated wind offset {x, z}; returns position
-- and half-length, or nil when the air is calm.
function Effects.streak(i, offset, wind)
  if math.abs(wind) < 0.15 then return nil end
  local x = wrap(-16 + (i * 37) % 32 + offset[1], -16, 16)
  local y = 1 + (i * 7.3) % 15
  local z = wrap(-16 + (i * 13.7) % 32 + offset[2], -16, 16)
  return {x, y, z}, 0.2 + math.abs(wind) * 0.25
end

return Effects
