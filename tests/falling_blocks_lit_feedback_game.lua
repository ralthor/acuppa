local root = board_file
local Board = assert(loadfile(root .. "/scripts/board.lua"))()
local Feedback = assert(loadfile(root .. "/scripts/clear_feedback.lua"))()
local real_new, real_spawn = Board.new, Board.spawn
local line_count, gravity_lock

Board.new = function(seed)
  local board = real_new(seed)
  for y = 1, line_count do
    for x = 2, Board.width do board.rows[y][x] = "T" end
  end
  return board
end

Board.spawn = function(self, ...)
  if not self.test_spawned then
    self.test_spawned = true
    self.active = {kind = "I", x = -2, y = gravity_lock and 0 or 16, rotation = 1}
    return true
  end
  return real_spawn(self, ...)
end

package.preload["scripts.board"] = function() return Board end
package.preload["scripts.clear_feedback"] = function() return Feedback end
package.preload["scripts.font"] = function()
  return {write = function(_, x, y, z, size, add) add(x, y, z, size) end}
end

local entities, sounds, actions, camera, next_id
package.preload["acuppa"] = function()
  return {
    spawn = function(data)
      next_id = next_id + 1
      entities[next_id] = data
      return next_id
    end,
    entity_destroy = function(id)
      assert(entities[id])
      entities[id] = nil
    end,
    component_set = function(id, kind, value)
      assert(entities[id] and kind == "transform")
      for key, item in pairs(value) do entities[id][key] = item end
    end,
    action = function(name) return actions[name] or 0 end,
    sound_play = function(path, options)
      local file = assert(io.open(root .. "/" .. path, "rb"))
      file:close()
      sounds[#sounds + 1] = {path = path, options = options}
      return #sounds
    end,
    set_camera = function(value) camera = value end,
    set_exposure = function() end,
    set_environment = function() end,
    set_ssao = function() end,
    scene_name = function() return "main" end,
  }
end

local function count_at(z)
  local count = 0
  for _, entity in pairs(entities) do
    if entity.position and entity.position[3] == z then count = count + 1 end
  end
  return count
end

local function run_case(lines, with_gravity)
  line_count, gravity_lock = lines, with_gravity
  entities, sounds, actions, camera, next_id = {}, {}, {}, nil, 0
  local game = assert(loadfile(root .. "/scripts/play.lua"))()
  game.start()
  assert(count_at(0.65) == 0 and #sounds == 0)
  if with_gravity then
    for _ = 1, 43 do game.fixed_update(1 / 60) end
  else
    actions.hard_drop = 1
    game.update(0)
  end
  local profile = Feedback.profile(lines)
  assert(count_at(0.65) == lines)
  assert(count_at(0.9) == lines * profile.sparks)
  assert(sounds[1].path == "assets/clear_" .. lines .. ".wav" and
         sounds[1].options.volume == profile.volume and
         sounds[1].options.pitch == profile.pitch)
  assert(#sounds == 1 and camera.eye[2] == -profile.shake and
         camera.target[2] == camera.eye[2])
  game.update(1 / 60)
  assert(math.abs(camera.eye[1]) + math.abs(camera.eye[2]) > 0.02 and
         camera.target[1] == camera.eye[1] and camera.target[2] == camera.eye[2])
  if lines == 4 then
    actions.restart = 1
    game.update(0)
    assert(count_at(0.65) == 0 and count_at(0.9) == 0)
    assert(camera.eye[1] == 0 and camera.eye[2] == 0)
    return
  end
  for _ = 1, 35 do game.update(1 / 60) end
  assert(count_at(0.65) == 0 and count_at(0.9) == 0)
  assert(camera.eye[1] == 0 and camera.eye[2] == 0)
end

run_case(1, false)
run_case(2, true)
run_case(3, false)
run_case(4, false)
line_count, gravity_lock = 0, false
entities, sounds, actions, camera, next_id = {}, {}, {hard_drop = 1}, nil, 0
local ordinary = assert(loadfile(root .. "/scripts/play.lua"))()
ordinary.start()
for i = 1, 4 do
  actions.hard_drop = 1
  ordinary.update(0)
  assert(sounds[i].path == "assets/land_" .. i .. ".wav" and
         sounds[i].options.volume >= 0.32 and sounds[i].options.volume <= 0.50)
  assert(camera.eye[2] == -Feedback.landing.shake and
         camera.target[2] == camera.eye[2])
  actions.hard_drop = 0
  ordinary.update(1 / 60)
end
assert(#sounds == 4 and count_at(0.65) == 0 and count_at(0.9) == 0)
for _ = 1, 10 do ordinary.update(1 / 60) end
assert(camera.eye[1] == 0 and camera.eye[2] == 0)

line_count, gravity_lock = 0, true
entities, sounds, actions, camera, next_id = {}, {}, {}, nil, 0
local soft = assert(loadfile(root .. "/scripts/play.lua"))()
soft.start()
for _ = 1, 43 do soft.fixed_update(1 / 60) end
assert(#sounds == 1 and sounds[1].path == "assets/land_1.wav")
assert(camera.eye[2] == -Feedback.landing.shake)
print("Falling Blocks Lit clear feedback passed through gameplay")
