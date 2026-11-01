-- Camera rig for Siege Sling: pure math, no engine calls.
-- Modes: "aim" sits behind and above the sling looking down the aim toward
-- the fort, "follow" chases the ball, and "fort" holds a view of the fort.
-- Every update eases the current pose toward the mode's pose, so switching
-- modes never jumps (snap() places it immediately).
local Rig = {}
Rig.__index = Rig

local RATES = {aim = 3.5, follow = 5, fort = 2.2}

local function finite(value)
  return type(value) == "number" and value == value and value > -math.huge and value < math.huge
end

local function vec3(where, name, value)
  if type(value) ~= "table" or not (finite(value[1]) and finite(value[2]) and finite(value[3])) then
    error(where .. ": " .. name .. " must hold three finite numbers", 3)
  end
  return {value[1], value[2], value[3]}
end

local function number(where, name, value)
  if not finite(value) then error(where .. ": " .. name .. " must be a finite number", 3) end
  return value
end

-- options: pouch (sling launch point), fort (fort center), distance (aim
-- camera distance, default 8), elevation (degrees, default 19), look_ahead
-- (meters along the aim, default 18), follow_distance (default 7).
function Rig.new(options)
  local where = "CameraRig.new"
  if type(options) ~= "table" then error(where .. ": options must be a table", 2) end
  local rig = setmetatable({
    pouch = vec3(where, "pouch", options.pouch), fort = vec3(where, "fort", options.fort),
    distance = number(where, "distance", options.distance or 8),
    elevation = number(where, "elevation", options.elevation or 19),
    look_ahead = number(where, "look_ahead", options.look_ahead or 18),
    follow_distance = number(where, "follow_distance", options.follow_distance or 7),
    heading = {0, 0, -1}, mode = "aim",
  }, Rig)
  if rig.distance <= 0 or rig.follow_distance <= 0 then
    error(where .. ": distances must be positive", 2)
  end
  return rig
end

-- Pose behind the sling. yaw is the shot yaw (degrees, + toward +X); the
-- camera turns 60% of it so the fort stays in view, plus the orbit offsets.
function Rig:aim_pose(yaw, orbit_yaw, orbit_pitch)
  local where = "rig:aim_pose"
  number(where, "yaw", yaw)
  number(where, "orbit_yaw", orbit_yaw or 0)
  number(where, "orbit_pitch", orbit_pitch or 0)
  local h = math.rad(yaw * 0.6 + (orbit_yaw or 0))
  local e = math.rad(math.max(5, math.min(60, self.elevation + (orbit_pitch or 0))))
  local fx, fz = math.sin(h), -math.cos(h)
  local p = self.pouch
  local eye = {p[1] - fx * math.cos(e) * self.distance, p[2] + math.sin(e) * self.distance,
               p[3] - fz * math.cos(e) * self.distance}
  local target = {p[1] + fx * self.look_ahead, p[2] - 1.8, p[3] + fz * self.look_ahead}
  return eye, target
end

function Rig:follow_pose(ball, velocity)
  local where = "rig:follow_pose"
  ball = vec3(where, "ball", ball)
  velocity = vec3(where, "velocity", velocity)
  local flat = math.sqrt(velocity[1] ^ 2 + velocity[3] ^ 2)
  if flat > 0.5 then self.heading = {velocity[1] / flat, 0, velocity[3] / flat} end
  local d = self.heading
  local eye = {ball[1] - d[1] * self.follow_distance, math.max(1, ball[2] + 2.5),
               ball[3] - d[3] * self.follow_distance}
  return eye, ball
end

function Rig:fort_pose()
  local f = self.fort
  return {f[1] + 2, f[2] + 3.5, f[3] + 12}, {f[1], f[2], f[3]}
end

local function desired(self, mode, info)
  if mode == "aim" then return self:aim_pose(info.yaw or 0, info.orbit_yaw, info.orbit_pitch) end
  if mode == "follow" then return self:follow_pose(info.ball, info.velocity) end
  if mode == "fort" then return self:fort_pose() end
  error("rig: unknown camera mode " .. tostring(mode), 3)
end

function Rig:snap(mode, info)
  if type(info) ~= "table" then error("rig:snap: info must be a table", 2) end
  self.eye, self.target = desired(self, mode, info)
  self.mode = mode
  return self.eye, self.target
end

-- Eases toward the pose for `mode` with rate-per-second smoothing.
function Rig:update(dt, mode, info)
  number("rig:update", "dt", dt)
  if dt < 0 or dt > 1 then error("rig:update: dt must be in [0, 1]", 2) end
  if type(info) ~= "table" then error("rig:update: info must be a table", 2) end
  local eye, target = desired(self, mode, info)
  if not self.eye then return self:snap(mode, info) end
  local blend = 1 - math.exp(-RATES[mode] * dt)
  for i = 1, 3 do
    self.eye[i] = self.eye[i] + (eye[i] - self.eye[i]) * blend
    self.target[i] = self.target[i] + (target[i] - self.target[i]) * blend
  end
  self.mode = mode
  return self.eye, self.target
end

return Rig
