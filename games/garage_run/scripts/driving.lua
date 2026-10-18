-- Deterministic arcade driving rules. Physics supplies the actual forward speed
-- each tick, so collisions reduce motion instead of being ignored by the controls.
local Drive = {}
Drive.__index = Drive

local function clamp(value, low, high)
  return math.max(low, math.min(high, value))
end

function Drive.new()
  return setmetatable({heading = 0, speed = 0, elapsed = 0, stage = 1,
                       in_parking = false, finished = false, hits = 0}, Drive)
end

function Drive:step(dt, throttle, steer, brake, actual_speed)
  assert(type(dt) == "number" and dt > 0 and dt <= 0.25)
  assert(type(throttle) == "number" and throttle >= -1 and throttle <= 1)
  assert(type(steer) == "number" and steer >= -1 and steer <= 1)
  assert(type(actual_speed) == "number" and math.abs(actual_speed) <= 1000)
  assert(type(brake) == "boolean")
  if self.finished then return 0, 0 end
  self.elapsed = self.elapsed + dt
  self.speed = clamp(actual_speed + throttle * 7 * dt, -3, 7)
  if throttle == 0 then self.speed = self.speed * math.max(0, 1 - 1.8 * dt) end
  if brake then self.speed = self.speed * math.max(0, 1 - 12 * dt) end
  if math.abs(self.speed) < 0.03 then self.speed = 0 end
  local direction = self.speed < 0 and -1 or 1
  self.heading = self.heading + steer * direction *
                 (0.45 + 0.14 * math.abs(self.speed)) * dt
  return -math.sin(self.heading) * self.speed,
         -math.cos(self.heading) * self.speed
end

function Drive:enter(name)
  if self.finished then return nil end
  if name == "checkpoint_1" and self.stage == 1 then
    self.stage = 2
    return "checkpoint"
  end
  if name == "checkpoint_2" and self.stage == 2 then
    self.stage = 3
    return "checkpoint"
  end
  if name == "parking" then self.in_parking = true end
  return nil
end

function Drive:leave(name)
  if name == "parking" then self.in_parking = false end
end

function Drive:hit()
  self.hits = self.hits + 1
end

function Drive:park()
  if self.finished or self.stage ~= 3 or not self.in_parking or
     math.abs(self.speed) > 1.1 then return false end
  self.finished = true
  return true
end

return Drive
