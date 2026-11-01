local tp = require("acuppa")
local ids, peaks, landed = {}, {}, {}
local ticks = 0

local function near(a, b) return math.abs(a - b) < 1e-5 end

local function material(id)
  return tp.component_get(id, "physics")
end

return {
  start = function()
    for _, name in ipairs({"ground", "bouncy", "dead", "switched", "slick", "grippy",
                           "ram", "crate", "drifter", "ccd_shot", "plain_shot"}) do
      ids[name] = assert(tp.entity_find(name))
    end
    -- Omitted fields use Jolt's defaults, so older scenes behave as before.
    local base = material(ids.ground)
    assert(base.density == 1000 and near(base.friction, 0.2) and base.restitution == 0)
    assert(near(base.linear_damping, 0.05) and near(base.angular_damping, 0.05))
    assert(base.ccd == false)
    local bouncy = material(ids.bouncy)
    assert(near(bouncy.restitution, 0.8) and near(bouncy.friction, 0.6) and
           bouncy.density == 500)
    assert(material(ids.ccd_shot).ccd == true and material(ids.plain_shot).ccd == false)
    assert(material(ids.drifter).linear_damping == 0)

    -- Partial sets keep current values; changed values rebuild the body.
    tp.component_set(ids.bouncy, "physics", {})
    assert(near(material(ids.bouncy).restitution, 0.8))
    tp.component_set(ids.switched, "physics", {restitution = 0})
    tp.component_set(ids.grippy, "physics", {friction = 1})
    tp.component_set(ids.drifter, "physics", {linear_damping = 3, angular_damping = 0.5})
    assert(material(ids.drifter).linear_damping == 3 and
           material(ids.drifter).angular_damping == 0.5)
    assert(near(tp.physics_state(ids.grippy).velocity[1], 4))
    tp.component_set(ids.dead, "physics", {ccd = true})
    tp.component_set(ids.dead, "physics", {ccd = false, density = 2000})
    assert(material(ids.dead).ccd == false and material(ids.dead).density == 2000)

    -- Invalid values raise errors and leave the component and body unchanged.
    local before = material(ids.bouncy)
    for _, bad in ipairs({
      {density = 0}, {density = -1}, {density = math.huge}, {density = 0 / 0},
      {density = "heavy"}, {friction = -0.1}, {friction = math.huge}, {friction = "x"},
      {restitution = 1.5}, {restitution = -0.1}, {restitution = 0 / 0},
      {linear_damping = -1}, {linear_damping = {}}, {angular_damping = math.huge},
      {angular_damping = -0.5}, {ccd = "yes"}, {ccd = 1},
      {restitution = 0.2, friction = -1},
    }) do
      local ok, message = pcall(tp.component_set, ids.bouncy, "physics", bad)
      assert(not ok and tostring(message):find("physics%."), tostring(message))
    end
    local after = material(ids.bouncy)
    for _, field in ipairs({"density", "friction", "restitution", "linear_damping",
                            "angular_damping", "ccd"}) do
      assert(after[field] == before[field], field)
    end
    assert(tp.physics_state(ids.bouncy))
  end,
  fixed_update = function()
    ticks = ticks + 1
    for _, event in ipairs(tp.physics_contacts()) do
      if event.began and (event.first == ids.ground or event.second == ids.ground) then
        landed[event.first] = true
        landed[event.second] = true
      end
    end
    for _, name in ipairs({"bouncy", "dead", "switched"}) do
      local id = ids[name]
      if landed[id] then
        peaks[name] = math.max(peaks[name] or 0, tp.physics_state(id).position[2])
      end
    end
    if ticks == 60 then
      local drift = tp.physics_state(ids.drifter).velocity[1]
      assert(drift > 0 and drift < 1, "linear damping did not slow the drifter")
      assert(tp.physics_state(ids.ccd_shot).position[1] < 10,
             "CCD shot passed through the thin wall")
      assert(tp.physics_state(ids.plain_shot).position[1] > 10,
             "plain shot no longer tunnels; retune the scene")
    end
    if ticks == 230 then
      assert(peaks.bouncy > 1.0, "restitution did not bounce")
      assert(peaks.dead < 0.5 and peaks.switched < 0.5,
             "zero restitution bounced after a runtime change")
      local slick = tp.physics_state(ids.slick).position[1] + 10
      local grippy = tp.physics_state(ids.grippy).position[1] + 10
      assert(slick > grippy * 2 and grippy > 0.5, "runtime friction change had no effect")
      assert(tp.physics_state(ids.crate).position[1] + 8.5 > 2,
             "dense ram did not push the light crate")
    end
  end,
  update = function() end,
  draw = function()
    if ticks >= 230 then assert(peaks.bouncy) end
  end,
}
