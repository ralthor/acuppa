-- Checkpoint and parking rules are independent of the physics controller.
local Drive = {}
Drive.__index = Drive

function Drive.new()
  return setmetatable({elapsed = 0, stage = 1, in_parking = false,
                       finished = false, hits = 0}, Drive)
end

function Drive:tick(dt)
  assert(type(dt) == "number" and dt > 0 and dt <= 0.25)
  if not self.finished then self.elapsed = self.elapsed + dt end
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

function Drive:park(speed)
  assert(type(speed) == "number" and speed >= 0 and speed < math.huge)
  if self.finished or self.stage ~= 3 or not self.in_parking or speed > 1.1 then
    return false
  end
  self.finished = true
  return true
end

return Drive
