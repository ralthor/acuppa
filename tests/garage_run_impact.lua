local Impact = assert(loadfile(impact_file))()
local quiet = Impact.new()
assert(quiet:hit({0, 0, 1}, 0.8) == 0)
local still = quiet:step(1 / 60)
assert(still[1] == 0 and still[2] == 0 and still[3] == 0)

local front = Impact.new()
local side = Impact.new()
assert(front:hit({0, 0, 1}, 8) > 0)
assert(side:hit({1, 0, 0}, 8) > 0)
local a, b = front:step(1 / 60), side:step(1 / 60)
assert(a[3] < -0.01 and a[2] > 0 and b[1] < -0.01 and b[2] > 0)
assert(math.abs(a[1]) < 0.0001 and math.abs(b[3]) < 0.0001)
for _ = 1, 240 do a = front:step(1 / 60) end
assert(math.abs(a[1]) + math.abs(a[2]) + math.abs(a[3]) < 0.001)
for _, call in ipairs({
  function() quiet:hit({0, 0, 1}, -1) end,
  function() quiet:hit({0, 0, 1}, math.huge) end,
  function() quiet:hit({math.huge, 0, 1}, 3) end,
  function() quiet:step(0) end,
}) do assert(not pcall(call)) end
