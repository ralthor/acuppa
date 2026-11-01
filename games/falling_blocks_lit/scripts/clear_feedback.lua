local Feedback = {}

Feedback.landing = {duration = 0.14, shake = 0.17}

local profiles = {
  [1] = {duration = 0.29, shake = 0.34, volume = 0.52, pitch = 1.08, sparks = 4},
  [2] = {duration = 0.38, shake = 0.50, volume = 0.68, pitch = 1.00, sparks = 6},
  [3] = {duration = 0.48, shake = 0.68, volume = 0.84, pitch = 0.92, sparks = 8},
  [4] = {duration = 0.58, shake = 0.86, volume = 1.00, pitch = 0.84, sparks = 10},
}

function Feedback.profile(lines)
  assert(type(lines) == "number" and lines == math.floor(lines) and profiles[lines],
         "line clear must contain one to four rows")
  return profiles[lines]
end

return Feedback
