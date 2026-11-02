local tp = require("acuppa")
local Rules = require("scripts.rules")
local Rig = require("scripts.camera_rig")
local DragAim = require("scripts.drag_aim")
local Debris = require("scripts.debris")
local Session = require("scripts.session")

local DOTS = 24          -- arc preview spheres
local DOT_STEP = 0.07    -- seconds between preview dots
local SOUND_GAP = 0.09   -- seconds between repeats of one impact sound
local SOUNDS = {wood = "assets/knock.wav", stone = "assets/thud.wav", glass = "assets/clink.wav",
                tnt = "assets/knock.wav", ice = "assets/crack.wav", metal = "assets/clang.wav",
                rubber = "assets/boing.wav", brick = "assets/thud.wav"}
local CONFIG = Rules.config()

local level, entry, level_count  -- chosen level data, its index entry, and the level count
local game, rig, drag, debris
local aim                -- current aim (keys and drags edit it)
local preview            -- aim shown while a drag is held (nil = current aim)
local ball, pouch_ball
local dots = {}
local bodies = {}        -- entity id -> {kind = "block"|"target", id, material, children}
local chips = {}         -- debris piece serial -> entity id
local cooldown = {}      -- sound file -> seconds left
local orbit_yaw, orbit_pitch = 0, 0
local orbit_from
local held = {}
local best               -- saved {score, stars} or nil
local was_state
local blasts = {}        -- pending explosion centers (processed each tick)
local popup = {time = 0, text = ""}
local shown_combo = 0

local function edge(name)
  local down = tp.action(name) > 0
  local pressed = down and not held[name]
  held[name] = down
  return pressed
end

local function axis(negative, positive)
  return math.max(-1, math.min(1, positive - negative))
end

local function spawn(name, position, mesh, scale, physics, shadows)
  local id = tp.entity_create(name)
  tp.component_set(id, "transform", {position = position, scale = scale})
  tp.component_set(id, "mesh", {mesh = mesh, casts_shadows = shadows ~= false})
  if physics then tp.component_set(id, "physics", physics) end
  return id
end

local function cue(file, position, volume)
  if (cooldown[file] or 0) > 0 then return end
  cooldown[file] = SOUND_GAP
  tp.sound_play(file, {position = position, volume = math.max(0.1, math.min(1, volume))})
end

local function spawn_fort()
  for _, block in ipairs(level.blocks) do
    local material = Rules.MATERIALS[block.material]
    local physics = {motion = "dynamic", density = material.density, friction = material.friction,
                     restitution = material.restitution}
    local mesh, scale
    if block.shape == "sphere" then
      local r = block.radius
      physics.shape, physics.radius, physics.angular_damping = "sphere", r, 0.6
      mesh, scale = "assets/boulder_" .. block.material .. ".gltf", {r, r, r}
    else
      physics.shape, physics.half_extents = "box", block.half_extents
      mesh, scale = "assets/" .. block.material .. ".gltf", block.half_extents
    end
    local id = spawn(block.id, block.position, mesh, scale, physics)
    bodies[id] = {kind = "block", id = block.id, material = block.material, children = {},
                  mass = Rules.block_mass(block)}
  end
  local r = level.target_radius
  for _, target in ipairs(level.targets) do
    local id = spawn(target.id, target.position, "assets/grump.gltf", {r, r, r},
                     {motion = "dynamic", shape = "sphere", radius = r, density = 500,
                      friction = 0.7, angular_damping = 2.5})
    local children = {}
    for _, side in ipairs({-1, 1}) do
      local eye = tp.entity_create(target.id .. " eye")
      tp.component_set(eye, "transform", {parent = id, position = {side * 0.38, 0.27, 0.84},
                                          scale = {0.3, 0.3, 0.3}})
      tp.component_set(eye, "mesh", {mesh = "assets/eye.gltf"})
      local pupil = tp.entity_create(target.id .. " pupil")
      tp.component_set(pupil, "transform", {parent = id, position = {side * 0.36, 0.25, 1.08},
                                            scale = {0.13, 0.13, 0.13}})
      tp.component_set(pupil, "mesh", {mesh = "assets/pupil.gltf", casts_shadows = false})
      children[#children + 1], children[#children + 2] = eye, pupil
    end
    bodies[id] = {kind = "target", id = target.id, children = children,
                  mass = 4 / 3 * math.pi * r ^ 3 * 500}
  end
end

-- Removes a broken block or defeated grump and throws debris.
local function shatter(entity)
  local body = bodies[entity]
  local state = tp.physics_state(entity)
  bodies[entity] = nil
  for _, child in ipairs(body.children) do tp.entity_destroy(child) end
  tp.entity_destroy(entity)
  local kind = body.kind == "target" and "grump" or body.material
  local file = body.kind == "target" and "assets/pop.wav" or SOUNDS[body.material]
  cooldown[file] = 0
  cue(file, state.position, 0.9)
  local explosive = body.kind == "block" and game:explosive(body.id)
  if explosive then blasts[#blasts + 1] = state.position end
  local added, evicted = debris:burst(kind, state.position,
                                      explosive and 16 or (body.kind == "target" and 8 or 6),
                                      explosive and 5.5 or 3)
  for _, piece in ipairs(evicted) do
    if chips[piece.serial] then tp.entity_destroy(chips[piece.serial]) end
    chips[piece.serial] = nil
  end
  for _, piece in ipairs(added) do
    chips[piece.serial] = spawn("debris", piece.position, "assets/chip_" .. kind .. ".gltf",
                                {0.09, 0.09, 0.09}, nil, false)
  end
end

-- A crate blast pushes every nearby block and grump outward (a velocity kick
-- that falls off linearly with distance, so heavy stone moves less in
-- meters but just as fast) and damages them; broken crates queue more blasts.
local function explode(center)
  cooldown["assets/boom.wav"] = 0
  cue("assets/boom.wav", center, 1)
  rig:shake(1)
  for _, id in ipairs(tp.physics_overlap_sphere(center, CONFIG.blast_radius)) do
    local body = bodies[id]
    if body then
      local p = tp.physics_state(id).position
      local d = {p[1] - center[1], p[2] - center[2] + 0.5, p[3] - center[3]}
      local distance = math.sqrt((p[1] - center[1]) ^ 2 + (p[2] - center[2]) ^ 2 + (p[3] - center[3]) ^ 2)
      local kick = Rules.blast_falloff(distance) * CONFIG.blast_speed * body.mass
      local norm = math.max(1e-3, math.sqrt(d[1] ^ 2 + d[2] ^ 2 + d[3] ^ 2))
      if kick > 0 then
        tp.physics_apply_impulse(id, {d[1] / norm * kick, d[2] / norm * kick, d[3] / norm * kick})
      end
      if select(2, game:blast(body.id, distance)) then shatter(id) end
    end
  end
end

local function run_blasts()
  local rounds = 0
  while #blasts > 0 and rounds < 8 do
    rounds = rounds + 1
    explode(table.remove(blasts, 1))
  end
end

local function step_debris(dt)
  for _, piece in ipairs(debris:step(dt)) do
    if chips[piece.serial] then tp.entity_destroy(chips[piece.serial]) end
    chips[piece.serial] = nil
  end
  for _, piece in ipairs(debris.pieces) do
    local id = chips[piece.serial]
    if id then tp.component_set(id, "transform", {position = piece.position, rotation = piece.spin}) end
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

local function remove_ball()
  if ball then tp.entity_destroy(ball) end
  ball = nil
end

local function fire(shot)
  remove_ball()
  local velocity = game:launch(shot)
  local b = level.ball
  ball = spawn("ball", level.pouch, "assets/ball.gltf", {b.radius, b.radius, b.radius},
               {motion = "dynamic", shape = "sphere", radius = b.radius, velocity = velocity,
                density = b.density, friction = b.friction, restitution = b.restitution,
                linear_damping = 0, angular_damping = 3, ccd = true})
  cooldown["assets/twang.wav"] = 0
  cue("assets/twang.wav", level.pouch, 0.5 + 0.5 * shot.power)
  aim = shot
  preview = nil
end

local function length(v) return math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2) end

local function feed_contacts()
  for _, contact in ipairs(tp.physics_contacts()) do
    if contact.began and not contact.trigger then
      local speed = contact.closing_speed
      local with_ball = ball and (contact.first == ball or contact.second == ball)
      if with_ball then game:landed() end
      for _, id in ipairs({contact.first, contact.second}) do
        local body = bodies[id]
        if body and body.kind == "block" then
          local state = tp.physics_state(id)
          if speed > 1 then cue(SOUNDS[body.material], state.position, speed / 10) end
          if select(2, game:hit_block(body.id, speed)) then shatter(id) end
        elseif body then
          local state = tp.physics_state(id)
          if speed > 1 then cue("assets/thud.wav", state.position, speed / 14) end
          if select(2, game:hit_target(body.id, speed)) then shatter(id) end
        end
      end
      if speed > 6 and (with_ball or bodies[contact.first] or bodies[contact.second]) then
        rig:shake(math.min(1, (speed - 6) / 12))
      end
    end
  end
end

local function tracked_speeds()
  local speeds = {}
  for id, body in pairs(bodies) do
    if not tp.entity_exists(id) then
      bodies[id] = nil  -- removed by another script
    else
      local state = tp.physics_state(id)
      speeds[#speeds + 1] = length(state.velocity)
      if body.kind == "target" and game:target_height(body.id, state.position[2]) then shatter(id) end
    end
  end
  if ball then
    local state = tp.physics_state(ball)
    local f = level.fort_center
    local near = math.abs(state.position[1] - f[1]) < 12 and math.abs(state.position[3] - f[3]) < 12
    if state.position[2] < -5 then
      game:landed()
      remove_ball()
    elseif near then
      speeds[#speeds + 1] = length(state.velocity)
    elseif game.state == "settling" or (game.state ~= "flying" and length(state.velocity) < 0.5) then
      remove_ball()  -- rolled away from the fort: it cannot change the result
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
  tp.set_camera({eye = eye, target = target, fov = 40, far = 300})
end

local function record_result()
  if game.state ~= "won" then return end
  best = Session.record(entry.file, game.score, game.stars)
end

local function has_next() return Session.index < level_count end

local function next_level()
  if not has_next() then return end
  Session.index = Session.index + 1
  tp.scene_reload()
end

local function aim_input(dt)
  local x, y = tp.pointer()
  local event, pulled = drag:update(tp.action("drag") > 0, x, y)
  if event == "pull" then
    preview = pulled
  elseif event == "fire" then
    fire(pulled)
  elseif event == "cancel" then
    preview = nil
  end
  if game.state ~= "aiming" or drag.active then return end
  local input = {
    yaw = axis(math.max(tp.action("aim_left"), tp.action("aim_left_arrow")),
               math.max(tp.action("aim_right"), tp.action("aim_right_arrow"))) + tp.action("aim_x"),
    pitch = axis(math.max(tp.action("aim_down"), tp.action("aim_down_arrow")),
                 math.max(tp.action("aim_up"), tp.action("aim_up_arrow"))) - tp.action("aim_y"),
    power = axis(tp.action("power_down"), tp.action("power_up")),
  }
  for key, value in pairs(input) do input[key] = math.max(-1, math.min(1, value)) end
  aim = Rules.adjust_aim(aim, input, dt)
  if edge("fire") then fire(aim) end
end

local function orbit_input(dt)
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
end

local function stars_row(x, y, count)
  for i = 1, 3 do
    local lit = i <= count
    tp.ui_rect(x + (i - 1) * 64, y, 52, 52, lit and 1 or 0.35, lit and 0.78 or 0.38,
               lit and 0.12 or 0.42, 0.95)
    tp.ui_text(x + (i - 1) * 64 + 17, y + 12, "*", 3)
  end
end

return {
  start = function()
    if Session.index < 1 or Session.index > #Session.levels() then Session.index = 1 end
    level, entry, level_count = Session.load(Session.index)
    game = Rules.new_game(level)
    rig = Rig.new({pouch = level.pouch, fort = level.fort_center, look_ahead = 17})
    drag = DragAim.new(Rules)
    debris = Debris.new({cap = 64, life = 1.0, ground = 0.05})
    aim = {yaw = level.aim.yaw, pitch = level.aim.pitch, power = level.aim.power}
    best = Session.best(entry.file)
    spawn_fort()
    local b = level.ball.radius
    pouch_ball = spawn("pouch ball", level.pouch, "assets/ball.gltf", {b, b, b})
    for i = 1, DOTS do
      dots[i] = spawn("arc dot " .. i, level.pouch, "assets/dot.gltf", {0.09, 0.09, 0.09}, nil, false)
    end
    tp.set_exposure(0.95)
    tp.set_environment("assets/day.hdr", 0.7)
    tp.set_sky({zenith = {0.03, 0.17, 1.0}, horizon = {0.42, 0.66, 1.15}, ground = {0.25, 0.5, 0.15},
                sun_direction = {0.55, 1, 0.45}, sun_color = {14, 12, 9}, sun_size = 2})
    tp.set_ssao(0)
    held.fire = tp.action("fire") > 0  -- a held Space from "play again" must not fire
    was_state = game.state
    update_preview()
    show_camera(rig:snap("aim", camera_info()))
  end,

  fixed_update = function(dt)
    for file, left in pairs(cooldown) do cooldown[file] = math.max(0, left - dt) end
    feed_contacts()
    run_blasts()
    local speeds = tracked_speeds()
    run_blasts()
    if game.combo > shown_combo and game.combo >= 2 then
      popup.time, popup.text = 1.4, string.format("COMBO x%d  +%d",
        Rules.combo_multiplier(game.combo), game.last_points)
    end
    shown_combo = game.combo
    popup.time = math.max(0, popup.time - dt)
    if game.state == "aiming" then
      aim_input(dt)
    else
      drag.active, preview = false, nil
      if game:finished() then
        if edge("fire") then tp.scene_reload() end
        if game.state == "won" and edge("next_level") then next_level() end
      else
        held.fire = tp.action("fire") > 0
        game:tick(dt, speeds)
      end
    end
    if game.state ~= was_state then
      was_state = game.state
      record_result()
    end
    orbit_input(dt)
    step_debris(dt)
    update_preview()
  end,

  update = function(dt)
    if edge("exit") then
      tp.scene_switch("menu")
      return
    end
    if edge("restart") then tp.scene_reload() end
    show_camera(rig:update(math.min(dt, 0.1), camera_mode(), camera_info()))
  end,

  draw = function()
    local w, h = tp.viewport_size()
    local current = shown_aim()
    tp.ui_rect(0, 0, w, 44, 0.05, 0.12, 0.25, 0.38)
    tp.ui_rect(w - 330, h - 30, 330, 30, 0.05, 0.12, 0.25, 0.38)
    tp.ui_text(w - 316, h - 22, string.format("LEVEL %d  %s", Session.index, entry.name:upper()), 1)
    tp.ui_text(18, 12, string.format("SHOTS %d   SCORE %d   GRUMPS %d",
                                     game.shots_left, game.score, game.targets_left), 2)
    if best then
      tp.ui_text(w / 2 + 40, 12, string.format("BEST %d  %d STARS", best.score, best.stars), 2)
    end
    if game.state == "aiming" then
      tp.ui_text(w - 250, 6, string.format("POWER %3d%%", math.floor(current.power * 100 + 0.5)), 2)
      tp.ui_text(w - 250, 28, string.format("YAW %+3d  PITCH %2d", math.floor(current.yaw + 0.5),
                                            math.floor(current.pitch + 0.5)), 1)
      tp.ui_rect(0, h - 26, 560, 26, 0.05, 0.12, 0.25, 0.3)
      tp.ui_text(12, h - 18, "DRAG OR WASD AIM  Q/E POWER  SPACE FIRE  R RESTART  ESC MENU", 1)
    end
    if popup.time > 0 then
      local a = math.min(1, popup.time * 2)
      tp.ui_rect(w / 2 - 150, 66, 300, 40, 1, 0.55, 0.1, 0.75 * a)
      tp.ui_text(w / 2 - 132, 76, popup.text, 2)
    end
    if game:finished() then
      local x, y = w / 2 - 260, h / 2 - 150
      local won = game.state == "won"
      tp.ui_rect(x, y, 520, 300, 0.04, 0.1, 0.22, 0.82)
      tp.ui_text(x + 40, y + 28, won and "VICTORY!" or "OUT OF SHOTS", 4)
      tp.ui_text(x + 40, y + 92, string.format("SCORE %d   %d STARS", game.score, game.stars), 2)
      if won and game.bonus > 0 then
        tp.ui_text(x + 40, y + 120, string.format("INCLUDES %d SHOT BONUS", game.bonus), 1)
      end
      stars_row(x + 40, y + 142, game.stars)
      tp.ui_text(x + 260, y + 158, best and string.format("BEST %d", best.score) or "NO BEST YET", 2)
      -- Buttons: Enter activates the focused (first) one; Space plays again.
      local bx = x + 40
      if won and has_next() then
        if tp.ui_button("next", bx, y + 222, 150, 50, "NEXT LEVEL") then next_level() end
        bx = bx + 160
      end
      if tp.ui_button("again", bx, y + 222, 150, 50, "PLAY AGAIN") then tp.scene_reload() end
      if tp.ui_button("menu", bx + 160, y + 222, 120, 50, "MENU") then tp.scene_switch("menu") end
    end
  end,
}
