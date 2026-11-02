-- Siege Sling camera rig and drag aiming, tested without the engine.
local dir = rules_file
local Rules = assert(loadfile(dir .. "/rules.lua"))()
local Rig = assert(loadfile(dir .. "/camera_rig.lua"))()
local Drag = assert(loadfile(dir .. "/drag_aim.lua"))()
local Debris = assert(loadfile(dir .. "/debris.lua"))()
local Catalog = assert(loadfile(dir .. "/catalog.lua"))()
local Core = assert(loadfile(dir .. "/editor_core.lua"))()

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

-- Catalog ------------------------------------------------------------------------
local shape_ids = {}
for _, entry in ipairs(Catalog.SHAPES) do shape_ids[#shape_ids + 1] = entry.id end
assert(table.concat(shape_ids, ",") == "cube,small_cube,plank,long_plank,pillar,slab,boulder",
       "catalog shape palette order")
local seen = {}
for _, entry in ipairs(Catalog.MATERIALS) do
  assert(Rules.MATERIALS[entry.id], "catalog material missing from rules: " .. entry.id)
  assert(type(entry.name) == "string" and #entry.color == 3, "material display data " .. entry.id)
  seen[entry.id] = true
end
for name in pairs(Rules.MATERIALS) do assert(seen[name], "rules material missing from catalog: " .. name) end
local level = {name = "Catalog", shots = 1, stars = {1, 2, 3}, targets = {{id = "g", position = {0, 0.45, -5}}},
               blocks = {}}
for i, entry in ipairs(Catalog.SHAPES) do
  level.blocks[i] = Catalog.block(entry.id, Catalog.MATERIALS[(i - 1) % 8 + 1].id, "b" .. i, {i, 1, -5})
end
Rules.validate_level(level)
assert(level.blocks[7].shape == "sphere" and level.blocks[7].radius == 0.45 and not level.blocks[7].half_extents,
       "boulder block is a sphere")
local plank = Catalog.block("plank", "wood", "p", {0, 0, 0})
plank.half_extents[1] = 2
assert(Catalog.shape("plank").half_extents[1] == 0.6, "catalog blocks copy their sizes")
assert(Catalog.material("ice").name == "Ice" and Catalog.shape("pillar").half_extents[2] == 0.7)
fails(function() Catalog.shape("cone") end, "unknown shape cone")
fails(function() Catalog.material("lava") end, "unknown material lava")
fails(function() Catalog.block("cube", "lava", "x", {0, 0, 0}) end, "unknown material")
fails(function() Catalog.block("cube", "wood", "", {0, 0, 0}) end, "id")
fails(function() Catalog.block("cube", "wood", "x", {0, 0}) end, "position")

-- Editor core ---------------------------------------------------------------------
assert(Core.snap(0.12) == 0 and Core.snap(0.13) == 0.25 and Core.snap(-0.38) == -0.5
       and Core.snap(1.3, 0.5) == 1.5, "grid snap")
fails(function() Core.snap(0 / 0) end, "finite")
fails(function() Core.snap(1, 0) end, "positive step")
local h = {1.1, 0.12, 0.4}
local turned = Core.rotate_y(h)
assert(turned[1] == 0.4 and turned[2] == 0.12 and turned[3] == 1.1, "turn swaps X and Z")
local tipped = Core.tip({0.25, 0.7, 0.3})
assert(tipped[1] == 0.25 and tipped[2] == 0.3 and tipped[3] == 0.7, "tip lays a pillar along its longer axis")
assert(Core.tip({0.6, 0.1, 0.35})[2] == 0.6, "tip stands a plank on its end")
local tipped_z = Core.tip({0.2, 0.5, 0.6})
assert(tipped_z[2] == 0.6 and tipped_z[3] == 0.5, "tip swaps with the longer horizontal axis")
assert(Core.tip(Core.tip({0.25, 0.7, 0.3}))[2] == 0.7, "tipping twice restores")
fails(function() Core.rotate_y({1, 2}) end, "three finite")
assert(Core.half({shape = "sphere", radius = 0.45})[2] == 0.45, "sphere half")
-- Stack height: a plank on a pillar rests on its top face.
local pillar_top = Core.top({0, 0.7, -18}, {0.25, 0.7, 0.3})
assert(near(pillar_top, 1.4), "pillar top")
local spot = Core.place({0.13, 1.1, -17.9}, h, {top = pillar_top})
assert(spot[1] == 0.25 and near(spot[2], 1.52) and spot[3] == -18, "stack placement")
local ground = Core.place({1.37, 0, -18.1}, {0.35, 0.35, 0.35})
assert(ground[1] == 1.25 and ground[2] == 0.35 and ground[3] == -18, "ground placement")
assert(near(Core.top({0, 1, 0}, {1, 0.5, 0.2}, {0, 0, math.pi / 2}), 2), "rolled box top uses its X extent")
assert(near(Core.top({0, 1, 0}, {1, 0.5, 0.2}, {math.pi / 2, 0, 0}), 1.2), "pitched box top uses Z")
assert(near(Core.top({0, 1, 0}, {1, 0.5, 0.2}, {0, 1.0, 0}), 1.5), "yaw keeps the top")
assert(Core.top({0, 1, 0}, {0.4, 0.4, 0.4}, nil, true) == 1.4, "sphere top")
fails(function() Core.place({0, 0, 0}, h, {top = "x"}) end, "support")
-- Stability tracker.
local tr = Core.tracker()
local stable, moving = tr:update(1 / 60, {2, 0})
assert(not stable and moving == 1, "moving body")
for _ = 1, 89 do stable = tr:update(1 / 60, {0.01, 0}) end
assert(not stable, "not stable before 1.5 s")
stable = tr:update(1 / 60, {0.01})
assert(stable and tr.moving == 0, "stable after 1.5 s calm")
tr:update(1 / 60, {0.5})
assert(not tr.stable and tr.calm == 0, "motion restarts the calm timer")
tr:update(0.25, {}) ; tr:reset()
assert(not tr.stable and tr.calm == 0, "reset forgets calm time")
fails(function() tr:update(1, {}) end, "dt")
fails(function() tr:update(0.1, {0 / 0}) end, "finite")
fails(function() Core.tracker({speed = 0}) end, "positive")
-- Undo history.
local hist = Core.history(2)
local layout = {{id = "a", position = {1, 2, 3}}}
hist:push(layout)
layout[1].position[1] = 9
assert(hist.items[1][1].position[1] == 1, "history copies layouts")
hist:push({}); hist:push({{id = "c"}})
assert(hist:count() == 2 and hist:pop()[1].id == "c" and #hist:pop() == 0 and hist:pop() == nil,
       "history caps and pops newest first")
fails(function() Core.history(0) end, "limit")
fails(function() hist:push(3) end, "layout")
-- Stars, names, slugs, ids.
local stars = Core.suggest_stars({{material = "wood"}, {material = "metal"}}, 2, 4, Rules.MATERIALS, 500, 1000)
assert(stars[1] >= 500 and stars[2] > stars[1] and stars[3] > stars[2] and stars[1] % 500 == 0,
       "star suggestion ascends in 500s")
local empty = Core.suggest_stars({}, 0, 1, Rules.MATERIALS)
assert(empty[1] == 500 and empty[2] == 1000 and empty[3] == 1500, "empty level stars")
fails(function() Core.suggest_stars({{material = "lava"}}, 1, 1, Rules.MATERIALS) end, "unknown material")
fails(function() Core.suggest_stars({}, 1, 0, Rules.MATERIALS) end, "shots")
assert(Core.name(1) == "Pebble Keep" and Core.name(#Core.NAMES + 2) == "Custom Level 2", "names")
assert(Core.slug("Pebble Keep") == "pebble_keep" and Core.slug("Custom Level 3!") == "custom_level_3"
       and Core.slug("Index") == "index_level" and Core.slug("***") == "level", "slugs")
fails(function() Core.slug("") end, "name")
assert(Core.unique_id("wood", {wood_1 = true, wood_2 = true}) == "wood_3", "unique ids")
local p, r = Core.clean_pose({0.70000123, -0.0000004, -17.25049}, {0.0004, 1.5707963, -0.001})
assert(p[1] == 0.7 and p[2] == 0 and math.type(p[2]) == "float" and 1 / p[2] > 0 and p[3] == -17.25
       and r[1] == 0 and r[2] == 1.5708 and r[3] == 0, "clean pose")
assert(select(2, Core.clean_pose({0, 0, 0}, {0.001, 0, -0.001})) == nil, "near-zero rotation dropped")
local built = Core.build_level({
  {kind = "block", id = "stone_1", material = "stone", half_extents = {0.25, 0.7, 0.3},
   position = {0, 0.7, -18}, rotation = {0, 0.5, 0}},
  {kind = "block", id = "rubber_1", material = "rubber", shape = "sphere", radius = 0.45,
   position = {2, 0.45, -18}, rotation = {0.3, 0.2, 0.1}},
  {kind = "target", id = "grump_1", shape = "sphere", radius = 0.45, position = {1, 0.45, -18}},
}, {name = "Pebble Keep", shots = 3, stars = {500, 1000, 2000}}, {})
Rules.check_playable(built)
assert(#built.blocks == 2 and #built.targets == 1 and built.blocks[1].rotation[2] == 0.5
       and built.blocks[2].radius == 0.45 and not built.blocks[2].half_extents
       and built.fort_center[1] == 1 and built.targets[1].rotation == nil, "built level")

print("siege_sling_play_modules: ok")
