local Board = assert(loadfile(board_file))()

local function prepared(count, gap)
  local board = Board.new()
  for y = 1, 4 do
    if y <= count and y ~= gap then
      for x = 2, Board.width do board.rows[y][x] = "T" end
    end
  end
  board.active = {kind = "I", x = -2, y = 0, rotation = 1}
  assert(board:can_place(board.active))
  return board
end

for count = 1, 4 do
  local board = prepared(count)
  local cleared, rows = board:lock()
  assert(cleared == count and #rows == count)
  for i, row in ipairs(rows) do assert(row == i - 1) end
end

local separated = prepared(3, 2)
local count, rows = separated:lock()
assert(count == 2 and rows[1] == 0 and rows[2] == 2)

local falling = prepared(4)
local moved, cleared, fallen_rows = falling:fall()
assert(not moved and cleared == 4 and #fallen_rows == 4)

local dropped = prepared(4)
dropped.active.y = 16
local distance, dropped_count, dropped_rows = dropped:hard_drop()
assert(distance == 16 and dropped_count == 4 and dropped_rows[4] == 3)

local empty = Board.new()
assert(empty:spawn("O"))
local moved_again, no_clear, no_rows = empty:fall()
assert(moved_again and no_clear == 0 and no_rows == nil)

print("Falling Blocks Lit clear-row positions passed")
