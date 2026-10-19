local Feedback = {}

local profiles = {
  [1] = {duration = 0.22, shake = 0.07, volume = 0.45, pitch = 1.05, sparks = 0},
  [2] = {duration = 0.30, shake = 0.14, volume = 0.65, pitch = 0.95, sparks = 2},
  [3] = {duration = 0.40, shake = 0.27, volume = 0.85, pitch = 0.85, sparks = 4},
  [4] = {duration = 0.50, shake = 0.40, volume = 1.00, pitch = 0.75, sparks = 6},
}

function Feedback.profile(lines)
  assert(type(lines) == "number" and lines == math.floor(lines) and profiles[lines],
         "line clear must contain one to four rows")
  return profiles[lines]
end

return Feedback
