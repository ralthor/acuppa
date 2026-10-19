local tp = require("acuppa")
local car, ticks
return {
  start = function()
    car = assert(tp.entity_find("car"))
    assert(not pcall(tp.vehicle_attach, tp.entity_find("floor")))
    assert(not pcall(tp.vehicle_attach, car, {engine_torque = -1}))
    assert(not pcall(tp.vehicle_attach, car, {center_of_mass_y = 0.1}))
    assert(not pcall(tp.vehicle_attach, car, {center_of_mass_y = -1.1}))
    tp.vehicle_attach(car, {rear_grip = 0.75, center_of_mass_y = -0.2})
    assert(math.abs(tp.physics_state(car).position[2] - 0.75) < 0.001)
    assert(not pcall(tp.vehicle_attach, car))
    assert(tp.vehicle_state(car).wheels[1].grounded == false)
    assert(tp.vehicle_state(999) == nil)
    assert(not pcall(tp.vehicle_set_input, car, {steer = math.huge}))
    assert(not pcall(tp.vehicle_set_input, car, {brake = -1}))
    ticks = 0
  end,
  fixed_update = function()
    ticks = ticks + 1
    if ticks < 80 then
      tp.vehicle_set_input(car, {steer = 1})
    else
      if ticks == 80 then
        local state = tp.physics_state(car)
        assert(math.abs(state.position[3]) < 0.08)
        assert(math.abs(state.rotation[2]) < 0.08)
        assert(tp.vehicle_state(car).wheels[1].grounded)
        assert(math.abs(tp.vehicle_state(car).wheels[1].steer) > 0.1)
      end
      tp.vehicle_set_input(car, {throttle = 1})
    end
    if ticks == 210 then
      assert(tp.physics_state(car).position[3] > 1)
      assert(tp.vehicle_state(car).engine_rpm > 0)
    end
  end,
  update = function() end,
}
