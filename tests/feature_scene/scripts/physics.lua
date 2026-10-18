local tp = require("threepoint")
local ground, falling, trigger
local saw_trigger, saw_ground, saw_exit = false, false, false

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
    local temp = tp.entity_create("temporary body")
    tp.component_set(temp, "transform", {position = {10, 1, 0}})
    tp.component_set(temp, "physics", {motion = "kinematic", shape = "sphere", radius = 0.5})
    assert(tp.physics_state(temp))
    assert(not pcall(tp.component_remove, temp, "transform"))
    assert(tp.component_remove(temp, "physics"))
    assert(tp.physics_state(temp) == nil)
    tp.entity_destroy(temp)
  end,
  fixed_update = function()
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
