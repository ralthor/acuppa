local Drive = assert(loadfile(driving_file))()

local car = Drive.new()
assert(car.stage == 1 and car.speed == 0 and car.elapsed == 0)
local x, z = car:step(1 / 60, 1, 0, false, 0)
assert(x == 0 and z < 0 and car.speed > 0)
for _ = 1, 60 do x, z = car:step(1 / 60, 1, 1, false, car.speed) end
assert(car.heading > 0 and x < 0 and z < 0 and car.elapsed > 1)
local before_brake = car.speed
car:step(1 / 60, 0, 0, true, car.speed)
assert(car.speed < before_brake)
assert(car:enter("checkpoint_2") == nil and car.stage == 1)
assert(car:enter("checkpoint_1") == "checkpoint" and car.stage == 2)
assert(car:enter("checkpoint_1") == nil)
assert(car:enter("parking") == nil and not car:park())
car:leave("parking")
assert(not car.in_parking)
assert(car:enter("checkpoint_2") == "checkpoint" and car.stage == 3)
car:hit()
assert(car.hits == 1)
car:enter("parking")
assert(not car:park())
car.speed = 0.8
assert(car:park() and car.finished and not car:park())
assert(car:step(1 / 60, 1, 1, false, 0) == 0)

local invalid = Drive.new()
for _, call in ipairs({
  function() invalid:step(-1, 0, 0, false, 0) end,
  function() invalid:step(1 / 60, 2, 0, false, 0) end,
  function() invalid:step(1 / 60, 0, math.huge, false, 0) end,
  function() invalid:step(1 / 60, 0, 0, false, 0 / 0) end,
  function() invalid:step(1 / 60, 0, 0, false, math.huge) end,
  function() invalid:step(1 / 60, 0, 0, 0, 0) end,
}) do assert(not pcall(call)) end
