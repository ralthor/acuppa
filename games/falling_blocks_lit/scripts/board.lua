-- A 10x20 falling-block board. Coordinates start at (0, 0) in the lower left.
local Board = {}
Board.__index = Board
Board.width, Board.height = 10, 20
Board.kinds = {"I", "O", "T", "S", "Z", "J", "L"}

local shapes = {
  I = {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
  O = {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
  T = {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
  S = {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
  Z = {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
  J = {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
  L = {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
}

local function empty_row()
  return {}
end

function Board.new(seed)
  seed = seed or 16
  assert(type(seed) == "number" and seed == math.floor(seed) and
         seed > 0 and seed < 2147483647, "invalid random seed")
  local self = setmetatable({rows = {}, active = nil, game_over = false,
                            random_state = seed, queue = {}, held_kind = nil,
                            hold_used = false}, Board)
  for y = 1, Board.height do self.rows[y] = empty_row() end
  return self
end

local function refill(self)
  if #self.queue > 0 then return end
  local bag = {table.unpack(Board.kinds)}
  for i = #bag, 2, -1 do
    self.random_state = self.random_state * 48271 % 2147483647
    local j = self.random_state % i + 1
    bag[i], bag[j] = bag[j], bag[i]
  end
  self.queue = bag
end

function Board:peek_kind()
  refill(self)
  return self.queue[1]
end

function Board:next_kind()
  refill(self)
  return table.remove(self.queue, 1)
end

function Board:cells(piece)
  piece = piece or self.active
  assert(piece and shapes[piece.kind], "unknown or missing piece")
  local result = {}
  for _, cell in ipairs(shapes[piece.kind]) do
    local x, y = cell[1], cell[2]
    if piece.kind ~= "O" then
      for _ = 1, piece.rotation % 4 do x, y = 3 - y, x end
    end
    result[#result + 1] = {piece.x + x, piece.y + y}
  end
  return result
end

function Board:can_place(piece)
  for _, cell in ipairs(self:cells(piece)) do
    local x, y = cell[1], cell[2]
    if x < 0 or x >= Board.width or y < 0 or y >= Board.height or
       self.rows[y + 1][x + 1] then return false end
  end
  return true
end

function Board:spawn(kind, x, y)
  assert(not self.active, "a piece is already active")
  kind = kind or self:next_kind()
  assert(shapes[kind], "unknown piece kind")
  if self.game_over then return false end
  local piece = {kind = kind, x = x or 3, y = y or Board.height - 4, rotation = 0}
  if not self:can_place(piece) then
    self.game_over = true
    return false
  end
  self.active = piece
  return true
end

function Board:move(dx, dy)
  if not self.active or self.game_over then return false end
  assert(dx == math.floor(dx) and dy == math.floor(dy), "move must use grid cells")
  local trial = {kind = self.active.kind, x = self.active.x + dx,
                 y = self.active.y + dy, rotation = self.active.rotation}
  if not self:can_place(trial) then return false end
  self.active = trial
  return true
end

function Board:rotate()
  if not self.active or self.game_over then return false end
  local kicks = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0},
                 {0, 1}, {-1, 1}, {1, 1}, {-2, 1}, {2, 1}}
  for _, offset in ipairs(kicks) do
    local trial = {kind = self.active.kind, x = self.active.x + offset[1],
                   y = self.active.y + offset[2],
                   rotation = (self.active.rotation + 1) % 4}
    if self:can_place(trial) then self.active = trial; return true end
  end
  return false
end

function Board:ghost_cells()
  if not self.active then return {} end
  local ghost = {kind = self.active.kind, x = self.active.x,
                 y = self.active.y, rotation = self.active.rotation}
  repeat ghost.y = ghost.y - 1 until not self:can_place(ghost)
  ghost.y = ghost.y + 1
  return self:cells(ghost)
end

function Board:hold()
  if not self.active or self.game_over or self.hold_used then return false end
  local kind = self.active.kind
  self.active = nil
  self:spawn(self.held_kind)
  self.held_kind = kind
  self.hold_used = true
  return true
end

function Board:clear_lines()
  local cleared, y, rows = 0, 1, {}
  while y <= Board.height do
    local full = true
    for x = 1, Board.width do
      if not self.rows[y][x] then full = false; break end
    end
    if full then
      rows[#rows + 1] = y - 1 + cleared
      table.remove(self.rows, y)
      self.rows[Board.height] = empty_row()
      cleared = cleared + 1
    else
      y = y + 1
    end
  end
  return cleared, rows
end

function Board:lock()
  assert(self.active and not self.game_over, "no active piece to lock")
  local kind = self.active.kind
  for _, cell in ipairs(self:cells()) do
    self.rows[cell[2] + 1][cell[1] + 1] = kind
  end
  self.active = nil
  self.hold_used = false
  return self:clear_lines()
end

function Board:fall()
  if not self.active or self.game_over then return false, 0 end
  if self:move(0, -1) then return true, 0 end
  local cleared, rows = self:lock()
  return false, cleared, rows
end

function Board:hard_drop()
  assert(self.active and not self.game_over, "no active piece to drop")
  local distance = 0
  while self:move(0, -1) do distance = distance + 1 end
  local cleared, rows = self:lock()
  return distance, cleared, rows
end

function Board.line_score(cleared, level)
  local points = {[0] = 0, 100, 300, 500, 800}
  assert(type(cleared) == "number" and cleared == math.floor(cleared) and
         cleared >= 0 and cleared <= 4, "invalid line count")
  assert(type(level) == "number" and level == math.floor(level) and level >= 1,
         "invalid level")
  return points[cleared] * level
end

function Board.level_for_lines(lines)
  assert(type(lines) == "number" and lines == math.floor(lines) and lines >= 0,
         "invalid total lines")
  return math.floor(lines / 10) + 1
end

function Board.gravity_interval(level, soft_drop)
  assert(type(level) == "number" and level == math.floor(level) and level >= 1,
         "invalid level")
  return soft_drop and 0.07 or math.max(0.08, 0.7 * 0.78 ^ (level - 1))
end

return Board
