local tp = require("acuppa")
local ground, falling, trigger, spinner
local saw_trigger, saw_ground, saw_exit = false, false, false
local spin_ticks = 0

return {
  start = function()
    ground, falling, trigger = tp.entity_find("ground"), tp.entity_find("falling"),
                               tp.entity_find("trigger")
    assert(tp.component_get(falling, "physics").motion == "dynamic")
    assert(tp.physics_state(falling).position[2] == 4)
    assert(tp.physics_raycast({0, 6, 0}, {0, -1, 0}, 10).id == falling)
    assert(#tp.physics_overlap_sphere({0, 2, 0}, 0.1) == 1)
    assert(not pcall(tp.physics_raycast, {0, 0, 0}, {0, 0, 0}, 1))
    assert(not pcall(tp.physics_overlap_sphere, {0, 0, 0}, 0))
    assert(not pcall(tp.physics_apply_force, ground, {0, 1, 0}))
    assert(not pcall(tp.physics_apply_impulse, ground, {0, 1, 0}))
    assert(not pcall(tp.physics_set_velocity, ground, {0, 1, 0}))
    assert(not pcall(tp.physics_move, 999, {0, 1, 0}))
    assert(not pcall(tp.physics_apply_force, falling, {math.huge, 0, 0}))
    assert(not pcall(tp.component_set, falling, "physics", {motion = "flying"}))
    assert(not pcall(tp.component_set, falling, "physics", {shape = "pyramid"}))
    assert(not pcall(tp.component_set, falling, "physics", {half_extents = {0, 1, 1}}))
    assert(not pcall(tp.component_set, falling, "physics", {trigger = "yes"}))
    assert(not pcall(tp.component_set, falling, "physics", {radius = -1}))
    assert(tp.component_get(falling, "physics").radius == 0.5)
    tp.physics_set_velocity(falling, {0, 0, 0})
    tp.physics_apply_force(falling, {0, 1, 0})
    tp.physics_apply_impulse(falling, {0, 0.01, 0})
    tp.physics_move(trigger, {0, 2, 0})
    -- An off-screen rod checks off-center forces and torque without changing the shot.
    spinner = tp.entity_create("spinner")
    tp.component_set(spinner, "transform", {position = {-50, 50, 0}})
    tp.component_set(spinner, "physics", {motion = "dynamic", shape = "box",
                                          half_extents = {0.2, 1, 0.2}})
    assert(tp.physics_state(spinner).angular_velocity[3] == 0)
    assert(not pcall(tp.physics_apply_force_at, ground, {1, 0, 0}, {0, 0, 0}))
    assert(not pcall(tp.physics_apply_force_at, spinner, {1, 0, 0}, {0, math.huge, 0}))
    assert(not pcall(tp.physics_apply_force_at, spinner, {1, 0, 0}))
    assert(not pcall(tp.physics_apply_torque, ground, {0, 1, 0}))
    assert(not pcall(tp.physics_apply_torque, spinner, {0 / 0, 0, 0}))
    local temp = tp.entity_create("temporary body")
    tp.component_set(temp, "transform", {position = {10, 1, 0}})
    tp.component_set(temp, "physics", {motion = "kinematic", shape = "sphere", radius = 0.5})
    assert(tp.physics_state(temp))
    tp.physics_set_pose(temp, {10, 2, 0}, {0, math.pi / 2, 0})
    assert(tp.physics_state(temp).position[2] == 2)
    assert(math.abs(tp.component_get(temp, "transform").rotation[2] - math.pi / 2) < 0.001)
    assert(not pcall(tp.physics_set_pose, temp, {0, 0, 0}, {math.huge, 0, 0}))
    assert(not pcall(tp.physics_set_pose, 999, {0, 0, 0}, {0, 0, 0}))
    assert(not pcall(tp.component_remove, temp, "transform"))
    assert(tp.component_remove(temp, "physics"))
    assert(tp.physics_state(temp) == nil)
    tp.entity_destroy(temp)
  end,
  fixed_update = function()
    spin_ticks = spin_ticks + 1
    if spin_ticks == 1 then
      -- Pushing +X below the center turns about +Z; the torque turns about -Y.
      tp.physics_apply_force_at(spinner, {2000, 0, 0}, {-50, 49, 0})
      tp.physics_apply_torque(spinner, {0, -300, 0})
    elseif spin_ticks == 2 then
      local spin = tp.physics_state(spinner).angular_velocity
      assert(spin[3] > 0.1 and spin[2] < -0.1, "off-center force or torque did not turn the rod")
      tp.entity_destroy(spinner)
    end
    for _, event in ipairs(tp.physics_contacts()) do
      if event.trigger and event.began and
         (event.first == trigger or event.second == trigger) then saw_trigger = true end
      if event.trigger and not event.began and
         (event.first == trigger or event.second == trigger) then saw_exit = true end
      if not event.trigger and event.began and
         (event.first == ground or event.second == ground) then saw_ground = true end
    end
    if tp.time() > 2 then assert(saw_trigger and saw_exit and saw_ground) end
  end,
  update = function() end,
  draw = function()
    assert(tp.physics_state(falling))
  end,
}
