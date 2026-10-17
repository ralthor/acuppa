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

print("Falling-block board rules passed")
