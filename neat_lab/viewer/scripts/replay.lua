-- Decodes the packed tracks in a trainer run file and samples them over time.
-- A track is "v,v,...;v,v,...": one row per 30 Hz frame, all rows the same width.
local Replay = {FPS = 30}

function Replay.decode(text, width, what)
  if type(text) ~= "string" then error((what or "track") .. " must be a string", 2) end
  local rows = {}
  for row in string.gmatch(text, "[^;]+") do
    local values = {}
    for item in string.gmatch(row, "[^,]+") do
      local value = tonumber(item)
      if not value or value ~= value then
        error((what or "track") .. " has a bad number: " .. item, 2)
      end
      values[#values + 1] = value
    end
    if #values ~= width then
      error(string.format("%s row %d has %d values, expected %d", what or "track",
                          #rows + 1, #values, width), 2)
    end
    rows[#rows + 1] = values
  end
  if #rows == 0 and width > 0 then error((what or "track") .. " is empty", 2) end
  return rows
end

function Replay.duration(rows)
  return math.max(0, #rows - 1) / Replay.FPS
end

-- Linearly interpolates between the two frames around time t (seconds);
-- before the start or after the end, the first or last frame holds.
function Replay.sample(rows, t)
  local position = math.max(0, t) * Replay.FPS + 1
  local index = math.min(#rows, math.floor(position))
  local nextrow = rows[math.min(#rows, index + 1)]
  local row, mix = rows[index], position - index
  if index >= #rows then return row, #rows end
  local out = {}
  for i = 1, #row do out[i] = row[i] + (nextrow[i] - row[i]) * mix end
  return out, index
end

return Replay
