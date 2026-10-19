local tp = require("acuppa")
local Drive = require("scripts.driving")
local Impact = require("scripts.impact")

local car, sun, motor, skid, drive, impact, best
local checkpoint_1, checkpoint_2, parking
local wheels = {}
local barriers = {}
local bump_cooldown, finish_message = 0, 0
local restart_held = false

local function clamp(value, low, high)
  return math.max(low, math.min(high, value))
end

local function show_camera(state)
  local yaw = state.rotation[2]
  local behind_x, behind_z = -math.sin(yaw), -math.cos(yaw)
  local p = state.position
  local shake = impact:step(1 / 60)
  local eye = {p[1] + behind_x * 9 + shake[1], p[2] + 5.2 + shake[2],
               p[3] + behind_z * 9 + shake[3]}
  tp.set_camera({eye = eye, target = {p[1] + shake[1] * 0.25,
                 p[2] + 0.15 + shake[2] * 0.25, p[3] + shake[3] * 0.25},
                 fov = 58, far = 150})
  tp.sound_set_listener(eye, {-behind_x, -0.5, -behind_z}, {0, 1, 0})
  tp.component_set(sun, "transform", {position = {p[1], 0, p[3]}})
end

local function cue(file, position, volume)
  tp.sound_play(file, {position = position, volume = volume})
end

return {
  start = function()
    car = assert(tp.entity_find("car"))
    sun = assert(tp.entity_find("sun"))
    checkpoint_1 = assert(tp.entity_find("checkpoint_1"))
    checkpoint_2 = assert(tp.entity_find("checkpoint_2"))
    parking = assert(tp.entity_find("parking"))
    for i, name in ipairs({"wheel front left", "wheel front right",
                           "wheel rear left", "wheel rear right"}) do
      wheels[i] = assert(tp.entity_find(name))
    end
    for _, name in ipairs({"left wall", "right wall", "north wall", "south wall",
                           "obstacle one", "obstacle two", "obstacle three"}) do
      barriers[assert(tp.entity_find(name))] = true
    end
    tp.vehicle_attach(car, {half_track = 0.55, half_wheelbase = 0.72,
                            wheel_radius = 0.28, engine_torque = 350,
                            rear_grip = 0.75})
    drive = Drive.new()
    impact = Impact.new()
    local saved = tp.load("best")
    best = saved and saved.seconds or nil
    tp.set_exposure(0.9)
    tp.set_environment("assets/sunset.hdr", 0.6)
    tp.set_ssao(0)
    show_camera(assert(tp.physics_state(car)))
    motor = tp.sound_play("assets/motor.wav", {position = {0, 0.75, 16},
                                                  volume = 0.35, loop = true})
    skid = tp.sound_play("assets/skid.wav", {position = {0, 0.75, 16},
                                               volume = 0, loop = true})
  end,
  fixed_update = function(dt)
    local state = assert(tp.physics_state(car))
    local vehicle = assert(tp.vehicle_state(car))
    drive:tick(dt)
    bump_cooldown = math.max(0, bump_cooldown - dt)
    finish_message = math.max(0, finish_message - dt)
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
              if drive:enter(name) == "checkpoint" then
                cue("assets/checkpoint.wav", state.position, 1.0)
              end
            else
              drive:leave(name)
            end
          end
        elseif contact.began and barriers[other] then
          drive:hit()
          local normal = contact.first == car and contact.normal or
                         {-contact.normal[1], -contact.normal[2], -contact.normal[3]}
          local strength = impact:hit(normal, contact.closing_speed)
          if bump_cooldown == 0 and strength > 0 then
            cue("assets/bump.wav", state.position, 0.25 + strength * 0.9)
            bump_cooldown = 0.35
          end
        end
      end
    end

    local speed = math.sqrt(state.velocity[1]^2 + state.velocity[3]^2)
    local steer = clamp(tp.action("right") - tp.action("left") + tp.action("steer"), -1, 1)
    -- Keep full steering in the parking bay, but limit the wheel angle as speed rises.
    steer = steer * clamp(1 - speed / 8, 0.2, 1)
    local throttle = clamp(tp.action("throttle") - tp.action("reverse"), -1, 1)
    local brake = tp.action("brake") > 0 and 1 or 0
    local handbrake = tp.action("drift") > 0 and 1 or 0
    tp.vehicle_set_input(car, {throttle = throttle, steer = steer,
                               brake = brake, handbrake = handbrake})
    for i, wheel in ipairs(vehicle.wheels) do
      tp.component_set(wheels[i], "transform", {rotation = {wheel.rotation, wheel.steer, 0}})
    end
    speed = math.sqrt(state.velocity[1]^2 + state.velocity[2]^2 + state.velocity[3]^2)
    if drive:park(speed) then
      finish_message = 3
      cue("assets/checkpoint.wav", state.position, 1.0)
      if not best or drive.elapsed < best then
        best = drive.elapsed
        tp.save("best", {seconds = best, hits = drive.hits})
      end
    end
    tp.sound_set_position(motor, state.position)
    tp.sound_set_volume(motor, clamp(0.25 + speed * 0.04 + math.abs(throttle) * 0.16, 0, 1))
    tp.sound_set_pitch(motor, clamp(0.72 + vehicle.engine_rpm / 4200, 0.72, 2.0))
    local slip = math.max(math.abs(vehicle.wheels[3].lateral_slip),
                          math.abs(vehicle.wheels[4].lateral_slip))
    tp.sound_set_position(skid, state.position)
    tp.sound_set_volume(skid, clamp((slip - 0.4) * 0.35, 0, 0.7))
    show_camera(state)
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
    tp.ui_text(22, 52, "W/S DRIVE  A/D STEER  SHIFT DRIFT  SPACE BRAKE  R RESTART  ESC EXIT", 1)
    if best then tp.ui_text(w - 275, 14, "BEST " .. string.format("%.1f", best), 2) end
    if finish_message > 0 then
      tp.ui_rect(w / 2 - 255, 242, 510, 164, 0.02, 0.08, 0.07, 0.94)
      tp.ui_text(w / 2 - 210, 268, "PARKED!", 4)
      tp.ui_text(w / 2 - 210, 332, "KEEP DRIVING OR PRESS R", 2)
    elseif drive.stage == 3 and drive.in_parking then
      tp.ui_text(w / 2 - 158, 120, "BRAKE TO PARK", 3)
    elseif drive.stage == 3 then
      tp.ui_text(w / 2 - 190, 120, "PARK AHEAD - BRAKE EARLY", 2)
    end
  end,
}
