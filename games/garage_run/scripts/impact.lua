-- A damped camera spring driven by the direction and closing speed of a solid hit.
local Impact = {}
Impact.__index = Impact

function Impact.new()
  return setmetatable({x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0}, Impact)
end

function Impact:hit(normal, speed)
  assert(type(normal) == "table" and type(speed) == "number" and
         speed >= 0 and speed < math.huge)
  local nx, ny, nz = normal[1], normal[2], normal[3]
  assert(type(nx) == "number" and type(ny) == "number" and
         type(nz) == "number" and math.abs(nx) < math.huge and
         math.abs(ny) < math.huge and math.abs(nz) < math.huge)
  local strength = math.min(1, math.max(0, (speed - 1.2) / 9))
  self.vx = self.vx - nx * strength * 6
  self.vy = self.vy + strength * (2.5 - ny)
  self.vz = self.vz - nz * strength * 6
  return strength
end

function Impact:step(dt)
  assert(type(dt) == "number" and dt > 0 and dt <= 0.25)
  local damping, spring = 12, 65
  self.vx = self.vx + (-spring * self.x - damping * self.vx) * dt
  self.vy = self.vy + (-spring * self.y - damping * self.vy) * dt
  self.vz = self.vz + (-spring * self.z - damping * self.vz) * dt
  self.x, self.y, self.z = self.x + self.vx * dt,
                           self.y + self.vy * dt, self.z + self.vz * dt
  local distance = math.sqrt(self.x^2 + self.y^2 + self.z^2)
  if distance > 0.65 then
    local scale = 0.65 / distance
    self.x, self.y, self.z = self.x * scale, self.y * scale, self.z * scale
  end
  return {self.x, self.y, self.z}
end

return Impact
