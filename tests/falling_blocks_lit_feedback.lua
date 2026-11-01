local Feedback = assert(loadfile(board_file))()

assert(Feedback.landing.duration >= 0.10 and Feedback.landing.duration <= 0.20)
assert(Feedback.landing.shake > 0)
local previous
for lines = 1, 4 do
  local current = Feedback.profile(lines)
  assert(current.duration > 0 and current.shake > 0 and current.volume > 0 and
         current.pitch > 0 and current.sparks >= 0)
  assert(current.shake > Feedback.landing.shake and
         current.duration > Feedback.landing.duration)
  if previous then
    assert(current.duration > previous.duration and
           current.shake > previous.shake and
           current.volume > previous.volume and
           current.sparks > previous.sparks)
  end
  previous = current
end
for _, value in ipairs({0, 5, 1.5, -1, "four"}) do
  assert(not pcall(Feedback.profile, value))
end
assert(not pcall(Feedback.profile, nil))

print("Falling Blocks Lit feedback tiers passed")
