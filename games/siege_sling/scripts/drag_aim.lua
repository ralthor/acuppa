-- Mouse drag aiming for Siege Sling: pure state, no engine calls.
-- Feed the left button and pointer each tick. A press starts a pull; while
-- held, the pull previews an aim; release fires it, unless the pull is
-- shorter than the rules' cancel distance.
local Drag = {}
Drag.__index = Drag

function Drag.new(rules, config)
  if type(rules) ~= "table" or type(rules.aim_from_drag) ~= "function" then
    error("DragAim.new: rules module required", 2)
  end
  return setmetatable({rules = rules, config = config, active = false}, Drag)
end

-- Returns an event and an aim:
--   "start", nil       button pressed
--   "pull", aim|nil    held; aim is nil while the pull is still too short
--   "fire", aim        released after a real pull
--   "cancel", nil      released inside the cancel distance
--   nil                idle
function Drag:update(pressed, x, y)
  if type(pressed) ~= "boolean" then error("drag:update: pressed must be a boolean", 2) end
  if type(x) ~= "number" or type(y) ~= "number" or x ~= x or y ~= y
     or math.abs(x) == math.huge or math.abs(y) == math.huge then
    error("drag:update: pointer must be finite numbers", 2)
  end
  if not self.active then
    if not pressed then return nil end
    self.active, self.x, self.y = true, x, y
    return "start", nil
  end
  local aim = self.rules.aim_from_drag(x - self.x, y - self.y, self.config)
  if pressed then return "pull", aim end
  self.active = false
  if aim then return "fire", aim end
  return "cancel", nil
end

-- Pull offset in pixels while dragging, or nil.
function Drag:offset(x, y)
  if not self.active then return nil end
  return x - self.x, y - self.y
end

return Drag
