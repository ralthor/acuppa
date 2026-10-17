local Board = assert(loadfile(board_file))()

local function expect_error(action)
  assert(not pcall(action))
end

for _, kind in ipairs(Board.kinds) do
  local board = Board.new()
  assert(board:spawn(kind))
  local cells = board:cells()
  assert(#cells == 4)
  local seen = {}
  for _, cell in ipairs(cells) do
    local key = cell[1] .. ":" .. cell[2]
    assert(not seen[key] and cell[1] >= 0 and cell[1] < 10 and
           cell[2] >= 0 and cell[2] < 20)
    seen[key] = true
  end
  for _ = 1, 9 do assert(board:move(0, -1)) end
  cells = board:cells()
  for _ = 1, 4 do assert(board:rotate()) end
  local again = board:cells()
  for i = 1, 4 do
    assert(cells[i][1] == again[i][1] and cells[i][2] == again[i][2])
  end
  local distance, cleared = board:hard_drop()
  assert(distance >= 0 and cleared == 0 and board.active == nil)
  local locked = 0
  for y = 1, Board.height do
    for x = 1, Board.width do
      if board.rows[y][x] then
        assert(board.rows[y][x] == kind)
        locked = locked + 1
      end
    end
  end
  assert(locked == 4)
end

local wall = Board.new()
assert(wall:spawn("O", -1, 0))
assert(not wall:move(-1, 0) and not wall:move(0, -1))
assert(wall:move(1, 0))
assert(wall:lock() == 0)
assert(wall.rows[1][2] == "O" and wall.rows[2][3] == "O")
assert(wall:spawn("T", 3, 3))
wall.rows[6][6] = "I"
for x = 1, Board.width do wall.rows[6][x], wall.rows[7][x] = "I", "I" end
assert(not wall:rotate() and wall.active.rotation == 0)
expect_error(function() wall:spawn("I") end)
expect_error(function() wall:move(0.5, 0) end)

for count = 1, 2 do
  local board = Board.new()
  for y = 1, count do
    for x = 3, 10 do board.rows[y][x] = "T" end
  end
  assert(board:spawn("O", -1, 0))
  assert(board:lock() == count)
  assert(board.rows[1][1] == (count == 1 and "O" or nil))
  assert(board.rows[20][1] == nil)
end

local top = Board.new()
top.rows[18][5] = "Z"
assert(not top:spawn("O"))
assert(top.game_over and not top.active and not top:move(1, 0))
expect_error(function() Board.new():spawn("bad") end)
expect_error(function() Board.new():hard_drop() end)

assert(Board.line_score(1, 1) == 100 and Board.line_score(2, 2) == 600 and
       Board.line_score(4, 1) == 800)
expect_error(function() Board.line_score(5, 1) end)
assert(Board.level_for_lines(0) == 1 and Board.level_for_lines(9) == 1 and
       Board.level_for_lines(10) == 2 and Board.level_for_lines(99) == 10)
assert(Board.gravity_interval(2, false) < Board.gravity_interval(1, false))
assert(Board.gravity_interval(100, false) == 0.08)
assert(Board.gravity_interval(1, true) == 0.07)
expect_error(function() Board.level_for_lines(-1) end)
expect_error(function() Board.gravity_interval(0, false) end)

local queue_a, queue_b = Board.new(16), Board.new(16)
for _ = 1, 3 do
  local seen = {}
  for i = 1, 7 do
    local preview = queue_a:peek_kind()
    local kind = queue_a:next_kind()
    assert(kind == preview and kind == queue_b:next_kind() and not seen[kind])
    seen[kind] = true
  end
  for _, kind in ipairs(Board.kinds) do assert(seen[kind]) end
end
expect_error(function() Board.new(0) end)

local kick = Board.new()
assert(kick:spawn("I", 3, 4) and kick:rotate())
for _ = 1, 5 do assert(kick:move(-1, 0)) end
assert(kick.active.x == -2 and kick:rotate() and kick.active.x == 0)
for _, cell in ipairs(kick:cells()) do assert(cell[1] >= 0) end

local held = Board.new()
assert(held:spawn() and held.active.kind == "I")
local next_kind = held:peek_kind()
assert(held:hold() and held.held_kind == "I" and held.active.kind == next_kind)
assert(not held:hold())
held:hard_drop()
assert(held:spawn() and held:hold() and held.active.kind == "I")
assert(held.held_kind ~= "I")

local ghost = Board.new()
assert(ghost:spawn("O"))
local landing = ghost:ghost_cells()
assert(#landing == 4)
for _, cell in ipairs(landing) do assert(cell[2] == 0 or cell[2] == 1) end
ghost:hard_drop()
assert(#ghost:ghost_cells() == 0)
print("Falling-block board rules passed")
