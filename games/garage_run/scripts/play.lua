local tp = require("threepoint")
local Drive = require("scripts.driving")

local car, motor, drive, best
local checkpoint_1, checkpoint_2, parking
local barriers = {}
local bump_cooldown = 0
local restart_held = false

local function show_camera(position)
  tp.set_camera({eye = {position[1], 5.9, position[3] + 9},
                 target = {position[1], 0.55, position[3] - 2.5},
                 fov = 58, far = 90})
  tp.sound_set_listener({position[1], 5.9, position[3] + 9},
                        {0, -0.45, -1}, {0, 1, 0})
end

local function cue(file, position, volume)
  tp.sound_play(file, {position = position, volume = volume})
end

return {
  start = function()
    car = assert(tp.entity_find("car"))
    checkpoint_1 = assert(tp.entity_find("checkpoint_1"))
    checkpoint_2 = assert(tp.entity_find("checkpoint_2"))
    parking = assert(tp.entity_find("parking"))
    for _, name in ipairs({"left wall", "right wall", "obstacle"}) do
      barriers[assert(tp.entity_find(name))] = true
    end
    drive = Drive.new()
    local saved = tp.load("best")
    best = saved and saved.seconds or nil
    tp.set_exposure(0.9)
    tp.set_environment("assets/sunset.hdr", 0.6)
    tp.set_ssao(2, 0.4, 0.6)
    show_camera({0, 0.38, 8})
    motor = tp.sound_play("assets/motor.wav", {position = {0, 0.38, 8},
                                                  volume = 0.45, loop = true})
  end,
  fixed_update = function(dt)
    local state = assert(tp.physics_state(car))
    bump_cooldown = math.max(0, bump_cooldown - dt)
    for _, contact in ipairs(tp.physics_contacts()) do
      local other = contact.first == car and contact.second or
                    (contact.second == car and contact.first or nil)
      if other then
        if contact.trigger then
          local name = other == checkpoint_1 and "checkpoint_1" or
                       (other == checkpoint_2 and "checkpoint_2" or
                        (other == parking and "parking" or nil))
          if name then
            if contact.began then
              local result = drive:enter(name)
              if result == "checkpoint" then
                cue("assets/checkpoint.wav", state.position, 1.0)
              end
            else
              drive:leave(name)
            end
          end
        elseif contact.began and barriers[other] then
          drive:hit()
          if bump_cooldown == 0 then
            cue("assets/bump.wav", state.position, 1.0)
            bump_cooldown = 0.35
          end
        end
      end
    end

    if not drive.finished then
      local forward_x, forward_z = -math.sin(drive.heading), -math.cos(drive.heading)
      local actual_speed = state.velocity[1] * forward_x + state.velocity[3] * forward_z
      local steer = math.max(-1, math.min(1, tp.action("left") - tp.action("right") -
                             tp.action("steer")))
      local throttle = math.max(-1, math.min(1, tp.action("throttle") -
                                tp.action("reverse")))
      local vx, vz = drive:step(dt, throttle, steer, tp.action("brake") > 0,
                                actual_speed)
      tp.physics_set_pose(car, state.position, {0, drive.heading, 0})
      tp.physics_set_velocity(car, {vx, state.velocity[2], vz})
      if drive:park() then
        tp.physics_set_velocity(car, {0, 0, 0})
        cue("assets/checkpoint.wav", state.position, 1.0)
        if not best or drive.elapsed < best then
          best = drive.elapsed
          tp.save("best", {seconds = best, hits = drive.hits})
        end
      end
    end
    tp.sound_set_position(motor, state.position)
    tp.sound_set_volume(motor, drive.finished and 0.12 or
                        (0.35 + 0.07 * math.abs(drive.speed)))
    show_camera(state.position)
  end,
  update = function()
    if tp.action("exit") > 0 then tp.quit() end
    local restart = tp.action("restart") > 0
    if restart and not restart_held then tp.scene_reload() end
    restart_held = restart
  end,
  draw = function()
    local w = tp.viewport_size()
    tp.ui_rect(0, 0, w, 90, 0.025, 0.045, 0.065, 0.88)
    local stage = drive.stage == 1 and "GATE 1" or
                  (drive.stage == 2 and "GATE 2" or "PARK")
    tp.ui_text(22, 14, "GARAGE RUN  " .. stage ..
               "  TIME " .. string.format("%.1f", drive.elapsed) ..
               "  HITS " .. drive.hits, 2)
    tp.ui_text(22, 52, "W/S DRIVE  A/D STEER  SPACE BRAKE  R RESTART  ESC EXIT", 1)
    if best then tp.ui_text(w - 275, 14, "BEST " .. string.format("%.1f", best), 2) end
    if drive.finished then
      tp.ui_rect(w / 2 - 255, 242, 510, 164, 0.02, 0.08, 0.07, 0.94)
      tp.ui_text(w / 2 - 210, 268, "PARKED!", 4)
      tp.ui_text(w / 2 - 210, 332, "PRESS R TO DRIVE AGAIN", 2)
    elseif drive.stage == 3 and drive.in_parking then
      tp.ui_text(w / 2 - 158, 120, "BRAKE TO PARK", 3)
    end
  end,
}
