-- Siege Sling camera rig and drag aiming, tested without the engine.
local dir = rules_file
local Rules = assert(loadfile(dir .. "/rules.lua"))()
local Rig = assert(loadfile(dir .. "/camera_rig.lua"))()
local Drag = assert(loadfile(dir .. "/drag_aim.lua"))()
local Debris = assert(loadfile(dir .. "/debris.lua"))()

local function near(a, b, t) return math.abs(a - b) <= (t or 1e-6) end
local function dist(a, b)
  return math.sqrt((a[1] - b[1]) ^ 2 + (a[2] - b[2]) ^ 2 + (a[3] - b[3]) ^ 2)
end
local function fails(call, pattern)
  local ok, message = pcall(call)
  assert(not ok, "expected failure: " .. pattern)
  assert(tostring(message):find(pattern, 1, true), tostring(message))
end

-- Camera rig ------------------------------------------------------------------
local pouch, fort = {0, 3.3, 0}, {0, 1.2, -21}
local rig = Rig.new({pouch = pouch, fort = fort})
local eye, target = rig:aim_pose(0)
assert(eye[3] > pouch[3] + 5 and eye[2] > pouch[2] + 1 and near(eye[1], 0), "aim eye behind/above")
assert(target[3] < -15 and near(target[1], 0), "aim looks down -Z at the fort")
local right_eye, right_target = rig:aim_pose(30)
assert(right_target[1] > 0 and right_eye[1] < 0, "aim view turns with yaw")
local orbit_eye = rig:aim_pose(0, 40, 0)
assert(orbit_eye[1] < -2, "orbit yaw swings the camera")
local high_eye = rig:aim_pose(0, 0, 30)
local top_eye = rig:aim_pose(0, 0, 500)
assert(high_eye[2] > eye[2] and near(top_eye[2], pouch[2] + 8 * math.sin(math.rad(60))),
  "orbit pitch raises and clamps")

-- Snap, then easing toward the follow pose without jumping.
rig:snap("aim", {yaw = 0})
local start = {rig.eye[1], rig.eye[2], rig.eye[3]}
local info = {ball = {0, 6, -8}, velocity = {0, 2, -15}}
rig:update(1 / 60, "follow", info)
local step = dist(rig.eye, start)
assert(step > 0 and step < 1, "follow eases instead of jumping")
for _ = 1, 240 do rig:update(1 / 60, "follow", info) end
assert(near(rig.eye[3], -1, 1e-3) and near(rig.eye[2], 8.5, 1e-3) and near(rig.target[3], -8, 1e-3),
  "follow settles behind the ball")
-- A stalled ball keeps the last heading.
rig:follow_pose({0, 0.3, -20}, {0, 0, 0})
assert(near(rig.heading[3], -1), "heading kept when the ball stops")
local low = rig:follow_pose({0, -3, -20}, {0, -5, -1})
assert(low[2] >= 1, "follow eye stays above ground")
for _ = 1, 400 do rig:update(1 / 60, "fort", {}) end
local fort_eye, fort_target = rig:fort_pose()
assert(dist(rig.eye, fort_eye) < 1e-3 and dist(rig.target, fort_target) < 1e-3, "holds fort view")
for _ = 1, 400 do rig:update(1 / 60, "aim", {yaw = 0}) end
assert(dist(rig.eye, eye) < 1e-3 and rig.mode == "aim", "eases back to the aim view")

fails(function() Rig.new(nil) end, "options")
fails(function() Rig.new({pouch = {0, 0}, fort = fort}) end, "pouch")
fails(function() Rig.new({pouch = pouch, fort = fort, distance = 0}) end, "positive")
fails(function() rig:aim_pose(0 / 0) end, "yaw")
fails(function() rig:update(1 / 60, "spin", {}) end, "unknown camera mode")
fails(function() rig:update(-1, "aim", {}) end, "dt")
fails(function() rig:update(1 / 60, "aim", nil) end, "info")
fails(function() rig:update(1 / 60, "follow", {ball = {0, 0, 0}}) end, "velocity")

-- Drag aiming --------------------------------------------------------------------
local drag = Drag.new(Rules)
assert(drag:update(false, 0, 0) == nil and not drag.active, "idle")
assert(drag:update(true, 640, 400) == "start" and drag.active, "press starts")
local event, aim = drag:update(true, 645, 404)
assert(event == "pull" and aim == nil, "short pull previews nothing")
event, aim = drag:update(true, 620, 500)
assert(event == "pull" and aim.yaw > 0 and aim.pitch > 30, "pull previews the aim")
local dx, dy = drag:offset(620, 500)
assert(dx == -20 and dy == 100, "offset")
event, aim = drag:update(false, 620, 500)
assert(event == "fire" and near(aim.power, math.sqrt(20 ^ 2 + 100 ^ 2) / 240) and not drag.active,
  "release fires")
assert(drag:offset(0, 0) == nil, "no offset when idle")
drag:update(true, 100, 100)
event, aim = drag:update(false, 108, 104)
assert(event == "cancel" and aim == nil and not drag.active, "tiny release cancels")
local custom = Drag.new(Rules, Rules.config({cancel_drag = 2, max_drag = 50}))
custom:update(true, 0, 0)
event, aim = custom:update(false, 0, 50)
assert(event == "fire" and aim.power == 1 and aim.pitch == 70, "custom config")
fails(function() Drag.new({}) end, "rules module")
fails(function() drag:update(1, 0, 0) end, "pressed")
fails(function() drag:update(true, 0 / 0, 0) end, "pointer")
fails(function() drag:update(true, 0, math.huge) end, "pointer")

-- Camera shake ---------------------------------------------------------------------
local shaker = Rig.new({pouch = pouch, fort = fort})
shaker:snap("aim", {yaw = 0})
local calm_eye = {shaker:update(1 / 60, "aim", {yaw = 0})}
shaker:shake(0.8)
local shaken = shaker:update(1 / 60, "aim", {yaw = 0})
assert(dist(shaken, shaker.eye) > 0.005, "shake offsets the eye")
assert(dist(shaker.eye, calm_eye[1]) < 1e-9, "shake leaves the smoothed pose alone")
shaker:shake(0.2)
assert(shaker.shake_amount > 0.7, "a weaker shake does not override a stronger one")
for _ = 1, 120 do shaken = shaker:update(1 / 60, "aim", {yaw = 0}) end
assert(dist(shaken, shaker.eye) < 1e-9, "shake decays to nothing")
fails(function() shaker:shake(2) end, "strength")
fails(function() shaker:shake(0 / 0) end, "strength")

-- Debris ------------------------------------------------------------------------------
local pile = Debris.new({cap = 10, life = 1, ground = 0})
local added, evicted = pile:burst("wood", {0, 2, 0}, 6, 3)
assert(#added == 6 and #evicted == 0 and pile:count() == 6, "burst adds pieces")
local spread = {}
for _, piece in ipairs(added) do
  assert(piece.velocity[2] > 0, "pieces fly upward first")
  spread[#spread + 1] = math.atan(piece.velocity[3], piece.velocity[1])
end
assert(math.abs(spread[1] - spread[2]) > 0.5, "pieces fan out")
added, evicted = pile:burst("glass", {1, 1, 0}, 6, 2)
assert(#evicted == 2 and pile:count() == 10 and evicted[1].kind == "wood", "cap evicts oldest")
local lowest = math.huge
for _ = 1, 50 do
  pile:step(1 / 60)
  for _, piece in ipairs(pile.pieces) do lowest = math.min(lowest, piece.position[2]) end
end
assert(lowest >= 0 and pile:count() == 10, "pieces stay above ground until they expire")
local expired = pile:step(0.25)
for _ = 1, 3 do for _, p in ipairs(pile:step(0.25)) do expired[#expired + 1] = p end end
assert(#expired == 10 and pile:count() == 0, "pieces expire after their life")
local replay_a, replay_b = Debris.new(), Debris.new()
local a1 = replay_a:burst("stone", {0, 1, 0}, 4, 3)
local b1 = replay_b:burst("stone", {0, 1, 0}, 4, 3)
assert(a1[3].velocity[1] == b1[3].velocity[1], "bursts are deterministic")
fails(function() Debris.new({cap = 0}) end, "cap")
fails(function() Debris.new({life = -1}) end, "life")
fails(function() pile:burst(1, {0, 0, 0}, 4, 3) end, "kind")
fails(function() pile:burst("wood", {0, 0}, 4, 3) end, "center")
fails(function() pile:burst("wood", {0, 0, 0}, 40, 3) end, "count")
fails(function() pile:burst("wood", {0, 0, 0}, 4, 0) end, "speed")
fails(function() pile:step(-1) end, "dt")

print("siege_sling_play_modules: ok")
