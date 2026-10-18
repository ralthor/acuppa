local Drive = assert(loadfile(driving_file))()

local car = Drive.new()
assert(car.stage == 1 and car.elapsed == 0)
for _ = 1, 60 do car:tick(1 / 60) end
assert(math.abs(car.elapsed - 1) < 0.0001)
assert(car:enter("checkpoint_2") == nil and car.stage == 1)
assert(car:enter("checkpoint_1") == "checkpoint" and car.stage == 2)
assert(car:enter("parking") == nil and not car:park(0))
car:leave("parking")
assert(not car.in_parking)
assert(car:enter("checkpoint_2") == "checkpoint" and car.stage == 3)
car:hit()
assert(car.hits == 1)
car:enter("parking")
assert(not car:park(2))
assert(car:park(0.8) and car.finished and not car:park(0))
car:tick(1 / 60)
assert(math.abs(car.elapsed - 1) < 0.0001)

local invalid = Drive.new()
for _, call in ipairs({
  function() invalid:tick(-1) end,
  function() invalid:tick(math.huge) end,
  function() invalid:park(-1) end,
  function() invalid:park(0 / 0) end,
}) do assert(not pcall(call)) end
