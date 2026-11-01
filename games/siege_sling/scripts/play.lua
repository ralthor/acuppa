local tp = require("acuppa")
local Rules = require("scripts.rules")
local Rig = require("scripts.camera_rig")
local DragAim = require("scripts.drag_aim")
local level = require("levels.level1")

local DOTS = 24          -- arc preview spheres
local DOT_STEP = 0.07    -- seconds between preview dots

local game, rig, drag
local aim                -- current aim (keys and drags edit it)
local preview            -- aim shown while a drag is held (nil = current aim)
local ball, pouch_ball
local dots = {}
local bodies = {}        -- entity id -> {kind = "block"|"target", id = level id}
local entity_of = {}     -- level id -> entity id
local orbit_yaw, orbit_pitch = 0, 0
local orbit_from
local held = {}

local function edge(name)
  local down = tp.action(name) > 0
  local pressed = down and not held[name]
  held[name] = down
  return pressed
end

local function axis(negative, positive)
  return math.max(-1, math.min(1, positive - negative))
end

local function spawn(name, position, mesh, scale, physics)
  local id = tp.entity_create(name)
  tp.component_set(id, "transform", {position = position, scale = scale})
  tp.component_set(id, "mesh", {mesh = mesh})
  if physics then tp.component_set(id, "physics", physics) end
  return id
end

local function spawn_fort()
  for _, block in ipairs(level.blocks) do
    local material = Rules.MATERIALS[block.material]
    local id = spawn(block.id, block.position, "assets/" .. block.material .. ".gltf",
                     block.half_extents,
                     {motion = "dynamic", shape = "box", half_extents = block.half_extents,
                      density = material.density, friction = material.friction,
                      restitution = material.restitution})
    bodies[id] = {kind = "block", id = block.id}
    entity_of[block.id] = id
  end
  local r = level.target_radius
  for _, target in ipairs(level.targets) do
    local id = spawn(target.id, target.position, "assets/grump.gltf", {r, r, r},
                     {motion = "dynamic", shape = "sphere", radius = r, density = 500,
                      friction = 0.7, angular_damping = 2.5})
    bodies[id] = {kind = "target", id = target.id}
    entity_of[target.id] = id
    for _, side in ipairs({-1, 1}) do
      local eye = tp.entity_create(target.id .. " eye")
      tp.component_set(eye, "transform", {parent = id, position = {side * 0.38, 0.27, 0.84},
                                          scale = {0.3, 0.3, 0.3}})
      tp.component_set(eye, "mesh", {mesh = "assets/eye.gltf"})
      local pupil = tp.entity_create(target.id .. " pupil")
      tp.component_set(pupil, "transform", {parent = id, position = {side * 0.36, 0.25, 1.08},
                                            scale = {0.13, 0.13, 0.13}})
      tp.component_set(pupil, "mesh", {mesh = "assets/pupil.gltf", casts_shadows = false})
    end
  end
end

local function shown_aim() return preview or aim end

local function update_preview()
  local current = shown_aim()
  local aiming = game.state == "aiming"
  local points = {}
  if aiming then
    points = Rules.predict_arc(level.pouch, Rules.launch_velocity(current),
                               {step = DOT_STEP, max_points = DOTS + 1})
  end
  for i, dot in ipairs(dots) do
    local point = points[i + 1]
    tp.component_set(dot, "mesh", {visible = point ~= nil})
    if point then tp.component_set(dot, "transform", {position = point}) end
  end
  -- The waiting ball sits in the pouch, pulled back by the power.
  local yaw, pitch = math.rad(current.yaw), math.rad(current.pitch)
  local back = 0.25 + 0.65 * current.power
  local p = level.pouch
  tp.component_set(pouch_ball, "mesh", {visible = aiming})
  tp.component_set(pouch_ball, "transform", {position = {
    p[1] - math.cos(pitch) * math.sin(yaw) * back, p[2] - math.sin(pitch) * back,
    p[3] + math.cos(pitch) * math.cos(yaw) * back}})
end

local function fire(shot)
  if ball then tp.entity_destroy(ball) end
  local velocity = game:launch(shot)
  local b = level.ball
  ball = spawn("ball", level.pouch, "assets/ball.gltf", {b.radius, b.radius, b.radius},
               {motion = "dynamic", shape = "sphere", radius = b.radius, velocity = velocity,
                density = b.density, friction = b.friction, restitution = b.restitution,
                linear_damping = 0, angular_damping = 3, ccd = true})
  aim = shot
  preview = nil
end

local function length(v) return math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2) end

local function feed_contacts()
  for _, contact in ipairs(tp.physics_contacts()) do
    if contact.began and not contact.trigger then
      if ball and (contact.first == ball or contact.second == ball) then game:landed() end
      for _, id in ipairs({contact.first, contact.second}) do
        local body = bodies[id]
        if body and body.kind == "block" then
          game:hit_block(body.id, contact.closing_speed)
        elseif body then
          game:hit_target(body.id, contact.closing_speed)
        end
      end
    end
  end
end

local function tracked_speeds()
  local speeds = {}
  for id, body in pairs(bodies) do
    local state = tp.physics_state(id)
    if state then
      speeds[#speeds + 1] = length(state.velocity)
      if body.kind == "target" then game:target_height(body.id, state.position[2]) end
    end
  end
  if ball then
    local state = tp.physics_state(ball)
    local f = level.fort_center
    -- A ball rolling away across the meadow does not hold up the result.
    if state.position[2] < -5 then game:landed()
    elseif math.abs(state.position[1] - f[1]) < 14 and math.abs(state.position[3] - f[3]) < 14 then
      speeds[#speeds + 1] = length(state.velocity)
    end
  end
  return speeds
end

local function camera_info()
  local info = {yaw = shown_aim().yaw, orbit_yaw = orbit_yaw, orbit_pitch = orbit_pitch}
  if ball then
    local state = tp.physics_state(ball)
    info.ball, info.velocity = state.position, state.velocity
  end
  return info
end

local function camera_mode()
  if game.state == "aiming" then return "aim" end
  if game.state == "flying" and ball then return "follow" end
  return "fort"
end

local function show_camera(eye, target)
  tp.set_camera({eye = eye, target = target, fov = 45, far = 250})
end

return {
  start = function()
    game = Rules.new_game(level)
    rig = Rig.new({pouch = level.pouch, fort = level.fort_center})
    drag = DragAim.new(Rules)
    aim = {yaw = level.aim.yaw, pitch = level.aim.pitch, power = level.aim.power}
    spawn_fort()
    local b = level.ball.radius
    pouch_ball = spawn("pouch ball", level.pouch, "assets/ball.gltf", {b, b, b})
    for i = 1, DOTS do
      dots[i] = tp.entity_create("arc dot " .. i)
      tp.component_set(dots[i], "transform", {position = level.pouch, scale = {0.09, 0.09, 0.09}})
      tp.component_set(dots[i], "mesh", {mesh = "assets/dot.gltf", casts_shadows = false})
    end
    tp.set_exposure(0.95)
    tp.set_environment("assets/day.hdr", 0.7)
    tp.set_sky({zenith = {0.03, 0.17, 1.0}, horizon = {0.42, 0.66, 1.15}, ground = {0.25, 0.5, 0.15},
                sun_direction = {0.55, 1, 0.45}, sun_color = {14, 12, 9}, sun_size = 2})
    tp.set_ssao(0)
    update_preview()
    show_camera(rig:snap("aim", camera_info()))
  end,

  fixed_update = function(dt)
    feed_contacts()
    local speeds = tracked_speeds()
    if game.state == "aiming" then
      local x, y = tp.pointer()
      local event, pulled = drag:update(tp.action("drag") > 0, x, y)
      if event == "pull" then
        preview = pulled
      elseif event == "fire" then
        fire(pulled)
      elseif event == "cancel" then
        preview = nil
      end
      if game.state == "aiming" and not drag.active then
        local input = {
          yaw = axis(math.max(tp.action("aim_left"), tp.action("aim_left_arrow")),
                     math.max(tp.action("aim_right"), tp.action("aim_right_arrow"))) +
                tp.action("aim_x"),
          pitch = axis(math.max(tp.action("aim_down"), tp.action("aim_down_arrow")),
                       math.max(tp.action("aim_up"), tp.action("aim_up_arrow"))) -
                  tp.action("aim_y"),
          power = axis(tp.action("power_down"), tp.action("power_up")),
        }
        for key, value in pairs(input) do input[key] = math.max(-1, math.min(1, value)) end
        aim = Rules.adjust_aim(aim, input, dt)
        if edge("fire") then fire(aim) end
      end
    else
      drag.active, preview = false, nil
      held.fire = tp.action("fire") > 0
      game:tick(dt, speeds)
    end
    -- Right-drag or the right stick orbits the aim camera.
    local x, y = tp.pointer()
    if tp.action("orbit") > 0 then
      if orbit_from then
        orbit_yaw = math.max(-70, math.min(70, orbit_yaw - (x - orbit_from[1]) * 0.25))
        orbit_pitch = math.max(-14, math.min(35, orbit_pitch + (y - orbit_from[2]) * 0.15))
      end
      orbit_from = {x, y}
    else
      orbit_from = nil
    end
    orbit_yaw = math.max(-70, math.min(70, orbit_yaw - tp.action("orbit_x") * 90 * dt))
    orbit_pitch = math.max(-14, math.min(35, orbit_pitch + tp.action("orbit_y") * 45 * dt))
    update_preview()
  end,

  update = function(dt)
    if tp.action("exit") > 0 then tp.quit() end
    if edge("restart") then tp.scene_reload() end
    show_camera(rig:update(math.min(dt, 0.1), camera_mode(), camera_info()))
  end,

  draw = function()
    local w = tp.viewport_size()
    local current = shown_aim()
    tp.ui_rect(0, 0, w, 84, 0.04, 0.12, 0.22, 0.82)
    tp.ui_text(22, 12, string.format("SIEGE SLING  SHOTS %d  SCORE %d  GRUMPS %d",
                                     game.shots_left, game.score, game.targets_left), 2)
    tp.ui_text(22, 50, "DRAG OR WASD AIM  Q/E POWER  SPACE FIRE  R RESTART  ESC EXIT", 1)
    if game.state == "aiming" then
      tp.ui_text(w - 330, 12, string.format("POWER %3d%%", math.floor(current.power * 100 + 0.5)), 2)
      tp.ui_text(w - 330, 50, string.format("YAW %+3d  PITCH %2d",
                                            math.floor(current.yaw + 0.5),
                                            math.floor(current.pitch + 0.5)), 1)
    end
    if game.state == "won" then
      tp.ui_text(w / 2 - 200, 120, "ALL GRUMPS DOWN!  " .. game.stars .. " STARS", 2)
    elseif game.state == "lost" then
      tp.ui_text(w / 2 - 220, 120, "OUT OF SHOTS - PRESS R TO RETRY", 2)
    end
  end,
}
