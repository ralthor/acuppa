-- Siege Sling level editor. Physics stays on while you build, so a stack
-- that cannot stand falls over in front of you. Placement math, stability,
-- undo history, and level building live in scripts/editor_core.lua.
local tp = require("acuppa")
local Rules = require("scripts.rules")
local Catalog = require("scripts.catalog")
local Core = require("scripts.editor_core")
local Session = require("scripts.session")
local Rig = require("scripts.camera_rig")

local CONFIG = Rules.config()
local POUCH, FORT = {0, 3.3, 0}, {0, 1.5, -18}
local GRUMP_RADIUS = 0.45
local CAMERA = {yaw = 0, pitch = 18, distance = 22, target = {0, 1.5, -13}}
local SHAPES = #Catalog.SHAPES + 1          -- the last palette shape is the grump
local LEFT_W, RIGHT_X = 236, 1046           -- UI panels; the viewport lies between
local PANEL_X, ROW_Y, ROW_STEP = 1058, 56, 36

local E                 -- persistent editor state (kept in Session.editor)
local items = {}        -- entity id -> item {kind, id, material, shape, half_extents, radius}
local markers = {}      -- entity id -> red "moving or fallen" marker entity
local preview, preview_marker, select_marker
local preview_mesh
local tracker, rig
local held = {}
local hover             -- {position = {x,y,z}} where a click would place
local press             -- left press in select mode: {x, y, id}
local edit_pending = false
local message, message_time = "", 0
local confirm_clear, confirm_leave = 0, 0

local function edge(name)
  local down = tp.action(name) > 0
  local pressed = down and not held[name]
  held[name] = down
  return pressed
end

local function say(text, seconds)
  message, message_time = text, seconds or 3
end

local function length(v) return math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2) end

-- Items ------------------------------------------------------------------------

local function sorted_ids()
  local ids = {}
  for id in pairs(items) do ids[#ids + 1] = id end
  table.sort(ids)
  return ids
end

-- The current layout: item data plus live physics poses.
local function layout()
  local list = {}
  for _, id in ipairs(sorted_ids()) do
    local item = Core.copy(items[id])
    local state = tp.physics_state(id)
    item.position, item.rotation = state.position, state.rotation
    list[#list + 1] = item
  end
  return list
end

local function counts()
  local blocks, grumps = {}, 0
  for _, item in pairs(items) do
    if item.kind == "target" then grumps = grumps + 1 else blocks[#blocks + 1] = item end
  end
  return blocks, grumps
end

local function refresh_stars()
  if not E.settings.auto then return end
  local blocks, grumps = counts()
  E.settings.stars = Core.suggest_stars(blocks, grumps, E.settings.shots, Rules.MATERIALS,
                                        CONFIG.target_points, CONFIG.shot_bonus)
end

local function mesh_for(item)
  if item.kind == "target" then return "assets/grump.gltf" end
  if item.shape == "sphere" then return "assets/boulder_" .. item.material .. ".gltf" end
  return "assets/" .. item.material .. ".gltf"
end

local function spawn_item(item)
  local id = tp.entity_create(item.id)
  local half = item.kind == "target" and {GRUMP_RADIUS, GRUMP_RADIUS, GRUMP_RADIUS} or Core.half(item)
  tp.component_set(id, "transform", {position = item.position, rotation = item.rotation, scale = half})
  tp.component_set(id, "mesh", {mesh = mesh_for(item)})
  local physics
  if item.kind == "target" then
    physics = {motion = "dynamic", shape = "sphere", radius = GRUMP_RADIUS, density = 500,
               friction = 0.7, angular_damping = 2.5}
  else
    local m = Rules.MATERIALS[item.material]
    physics = {motion = "dynamic", density = m.density, friction = m.friction,
               restitution = m.restitution}
    if item.shape == "sphere" then
      physics.shape, physics.radius, physics.angular_damping = "sphere", item.radius, 0.6
    else
      physics.shape, physics.half_extents = "box", item.half_extents
    end
  end
  tp.component_set(id, "physics", physics)
  local stored = Core.copy(item)
  stored.position, stored.rotation = nil, nil
  items[id] = stored
  local marker = tp.entity_create(item.id .. " marker")
  tp.component_set(marker, "transform", {position = item.position, scale = {0.12, 0.12, 0.12}})
  tp.component_set(marker, "mesh", {mesh = "assets/marker_red.gltf", visible = false, casts_shadows = false})
  markers[id] = marker
  return id
end

local function clear_world()
  for id, marker in pairs(markers) do
    tp.entity_destroy(marker)
    tp.entity_destroy(id)
  end
  items, markers = {}, {}
  E.selected = nil
end

local function respawn(list)
  clear_world()
  for _, item in ipairs(list) do spawn_item(item) end
  tracker:reset()
  edit_pending = true
  refresh_stars()
end

-- Records the layout for undo before an edit.
local function before_edit()
  E.history:push(layout())
  E.dirty = true
end

local function after_edit()
  tracker:reset()
  edit_pending = true
  refresh_stars()
end

local function used_ids()
  local used = {}
  for _, item in pairs(items) do used[item.id] = true end
  return used
end

-- Palette ----------------------------------------------------------------------

local function current_item()
  if E.carry then return E.carry end
  if E.shape == SHAPES then return {kind = "target", shape = "sphere", radius = GRUMP_RADIUS} end
  local shape = Catalog.SHAPES[E.shape]
  local material = Catalog.MATERIALS[E.material].id
  if shape.shape == "sphere" then
    return {kind = "block", material = material, shape = "sphere", radius = shape.radius}
  end
  local h = {shape.half_extents[1], shape.half_extents[2], shape.half_extents[3]}
  if E.tipped then h = Core.tip(h) end
  if E.turned then h = Core.rotate_y(h) end
  return {kind = "block", material = material, half_extents = h}
end

local function over_ui(x, y)
  return x < LEFT_W or x > RIGHT_X or y < 48 or y > 688
end

local function update_hover()
  hover = nil
  local x, y = tp.pointer()
  local placing = E.mode == "place" or E.carry
  if placing and not over_ui(x, y) then
    local ray = tp.screen_ray(x, y)
    local hit = tp.physics_raycast(ray.origin, ray.direction, 300)
    if hit then
      local item = current_item()
      local half = Core.half(item)
      local support
      local below = items[hit.id]
      if below then
        local state = tp.physics_state(hit.id)
        local below_half = below.kind == "target" and {GRUMP_RADIUS, GRUMP_RADIUS, GRUMP_RADIUS}
          or Core.half(below)
        support = {top = Core.top(state.position, below_half, state.rotation, below.shape == "sphere")}
      end
      hover = {position = Core.place(hit.position, half, support), item = item}
    end
  end
  if hover then
    local mesh = mesh_for(hover.item)
    if mesh ~= preview_mesh then
      tp.component_set(preview, "mesh", {mesh = mesh})
      preview_mesh = mesh
    end
    local half = Core.half(hover.item)
    tp.component_set(preview, "transform", {position = hover.position, scale = half})
    tp.component_set(preview_marker, "transform", {position = {hover.position[1],
      hover.position[2] + half[2] + 0.35, hover.position[3]}})
  end
  tp.component_set(preview, "mesh", {visible = hover ~= nil})
  tp.component_set(preview_marker, "mesh", {visible = hover ~= nil})
  local selected = E.selected and items[E.selected] and E.selected
  if selected then
    local state = tp.physics_state(selected)
    local item = items[selected]
    local half = item.kind == "target" and {GRUMP_RADIUS, GRUMP_RADIUS, GRUMP_RADIUS} or Core.half(item)
    tp.component_set(select_marker, "transform", {position = {state.position[1],
      Core.top(state.position, half, state.rotation, item.shape == "sphere") + 0.45, state.position[3]}})
  end
  tp.component_set(select_marker, "mesh", {visible = selected ~= nil})
end

-- Edits ------------------------------------------------------------------------

local function place()
  if not hover then return end
  local item = Core.copy(hover.item)
  item.position = hover.position
  if E.carry then
    E.carry = nil  -- the history entry was recorded when it was picked up
  else
    before_edit()
    item.id = Core.unique_id(item.kind == "target" and "grump" or item.material, used_ids())
  end
  E.selected = spawn_item(item)
  after_edit()
end

local function remove(id)
  tp.entity_destroy(markers[id])
  tp.entity_destroy(id)
  items[id], markers[id] = nil, nil
  if E.selected == id then E.selected = nil end
end

local function grab(id)
  if not items[id] then return end
  before_edit()
  local item = Core.copy(items[id])
  remove(id)
  E.carry = item
  after_edit()
  say("MOVING " .. item.id:upper() .. ": CLICK TO PLACE, CTRL+Z TO CANCEL")
end

local function delete_selected()
  if not (E.selected and items[E.selected]) then
    say("SELECT A BLOCK FIRST (Q SWITCHES TO SELECT MODE)")
    return
  end
  before_edit()
  remove(E.selected)
  after_edit()
end

local function undo()
  local previous = E.history:pop()
  if not previous then
    say("NOTHING TO UNDO")
    return
  end
  E.carry = nil
  respawn(previous)
  E.dirty = true
  say("UNDONE")
end

local function reset_layout()
  before_edit()
  E.carry = nil
  respawn(E.snapshot)
  say("RESET TO THE LAST STABLE LAYOUT")
end

local function clear_all()
  if confirm_clear <= 0 then
    confirm_clear = 3
    say("CLICK CLEAR ALL AGAIN TO REMOVE EVERYTHING")
    return
  end
  confirm_clear = 0
  before_edit()
  E.carry = nil
  respawn({})
  say("CLEARED")
end

-- Levels -----------------------------------------------------------------------

local function settings_level()
  return Core.build_level(layout(), {name = E.settings.name, shots = E.settings.shots,
                                     stars = E.settings.stars}, E.base)
end

local function draft()
  local _, grumps = counts()
  if grumps == 0 then return nil, "ADD AT LEAST ONE GRUMP" end
  local level = settings_level()
  local ok, problem = pcall(Rules.check_playable, level)
  if not ok then return nil, tostring(problem):gsub("^.-: ", ""):upper() end
  return level
end

local function save()
  if E.carry then return say("CANNOT SAVE: PLACE THE BLOCK YOU ARE MOVING") end
  if not tracker.stable then
    return say(string.format("CANNOT SAVE: WAIT UNTIL STABLE (%d MOVING)", tracker.moving))
  end
  local level, problem = draft()
  if not level then return say("CANNOT SAVE: " .. problem) end
  local entries = Session.levels()
  local file = E.file
  if not file then
    local slug = Core.slug(E.settings.name)
    file = "levels/" .. slug .. ".lua"
    local n = 1
    local listed = {}
    for _, entry in ipairs(entries) do listed[entry.file] = true end
    while listed[file] or tp.data_read(file) ~= nil do
      n = n + 1
      file = "levels/" .. slug .. "_" .. n .. ".lua"
    end
  end
  tp.data_write(file, level)
  local found = false
  for _, entry in ipairs(entries) do
    if entry.file == file then entry.name, found = E.settings.name, true end
  end
  if not found then entries[#entries + 1] = {file = file, name = E.settings.name} end
  tp.data_write(Session.INDEX, entries)
  E.file, E.dirty = file, false
  say("SAVED " .. file:upper(), 4)
end

local function load_next()
  local entries = Session.levels()
  E.load_index = E.load_index % #entries + 1
  local level, entry = Session.load(E.load_index)
  before_edit()
  local list = {}
  for _, block in ipairs(level.blocks) do
    local item = Core.copy(block)
    item.kind = "block"
    if item.shape ~= "sphere" then item.shape = nil end
    list[#list + 1] = item
  end
  for _, target in ipairs(level.targets) do
    list[#list + 1] = {kind = "target", id = target.id, shape = "sphere", radius = GRUMP_RADIUS,
                       position = Core.copy(target.position)}
  end
  E.base = {pouch = level.pouch, aim = level.aim, ball = level.ball,
            target_radius = level.target_radius, kill_y = level.kill_y}
  E.settings = {name = entry.name, name_index = 0, shots = level.shots,
                stars = Core.copy(level.stars), auto = false}
  E.file, E.carry = entry.file, nil
  respawn(list)
  E.snapshot = list
  E.dirty = false
  say("LOADED " .. entry.name:upper() .. " (" .. entry.file:upper() .. ")")
end

local function new_name()
  E.settings.name_index = E.settings.name_index + 1
  E.settings.name = Core.name(E.settings.name_index)
end

local function play_test()
  if E.carry then return say("PLACE THE BLOCK YOU ARE MOVING FIRST") end
  local level, problem = draft()
  if not level then return say("CANNOT PLAY TEST: " .. problem) end
  E.items = layout()  -- the exact layout to restore afterwards
  Session.playtest = level
  tp.scene_switch("play")
end

local function leave()
  if E.dirty and confirm_leave <= 0 then
    confirm_leave = 3
    say("UNSAVED CHANGES: PRESS ESC AGAIN TO LEAVE WITHOUT SAVING")
    return
  end
  Session.editor = nil
  tp.scene_switch("menu")
end

-- A new level starts with a tiny example: two pillars, a plank, and a grump.
local function starter()
  return {
    {kind = "block", id = "stone_1", material = "stone", half_extents = {0.25, 0.7, 0.3},
     position = {-1, 0.7, -18}},
    {kind = "block", id = "stone_2", material = "stone", half_extents = {0.25, 0.7, 0.3},
     position = {1, 0.7, -18}},
    {kind = "block", id = "wood_1", material = "wood", half_extents = {1.1, 0.12, 0.4},
     position = {0, 1.52, -18}},
    {kind = "target", id = "grump_1", shape = "sphere", radius = GRUMP_RADIUS, position = {0, 0.45, -18}},
  }
end

local function new_state()
  return {items = starter(), settings = {name = Core.name(1), name_index = 1, shots = 4,
          stars = {500, 1000, 1500}, auto = true}, base = {}, file = nil, dirty = false,
          history = Core.history(50), snapshot = starter(), material = 1, shape = 1,
          mode = "place", turned = false, tipped = false, load_index = 0,
          camera = Core.copy(CAMERA), aim_view = false}
end

-- Camera -----------------------------------------------------------------------

local function camera_input(dt)
  local c = E.camera
  local ctrl = tp.action("ed_ctrl") > 0 or tp.action("ed_ctrl_right") > 0
  if edge("ed_view_reset") then E.camera, E.aim_view = Core.copy(CAMERA), false; c = E.camera end
  if edge("ed_aim_view") then E.aim_view = not E.aim_view end
  local x, y = tp.pointer()
  if tp.action("orbit") > 0 then
    if E.orbit_from then
      c.yaw = c.yaw - (x - E.orbit_from[1]) * 0.3
      c.pitch = math.max(5, math.min(80, c.pitch + (y - E.orbit_from[2]) * 0.2))
    end
    E.orbit_from = {x, y}
  else
    E.orbit_from = nil
  end
  c.yaw = c.yaw + (tp.action("aim_right_arrow") - tp.action("aim_left_arrow")) * 60 * dt
  c.pitch = math.max(5, math.min(80, c.pitch + (tp.action("aim_up_arrow") - tp.action("aim_down_arrow")) * 40 * dt))
  if not ctrl then
    local zoom = tp.action("ed_zoom_out") - tp.action("ed_zoom_in")
    c.distance = math.max(4, math.min(45, c.distance * (1 + zoom * 1.2 * dt)))
  end
  local yaw = math.rad(c.yaw)
  local forward, right = tp.action("aim_up") - tp.action("aim_down"), tp.action("aim_right") - tp.action("aim_left")
  local speed = 0.5 * c.distance * dt
  c.target[1] = c.target[1] + (math.cos(yaw) * right - math.sin(yaw) * forward) * speed
  c.target[3] = c.target[3] + (-math.sin(yaw) * right - math.cos(yaw) * forward) * speed
  if E.aim_view then
    local eye, target = rig:aim_pose(0)
    tp.set_camera({eye = eye, target = target, fov = 40, far = 300})
  else
    local pitch = math.rad(c.pitch)
    local t = c.target
    tp.set_camera({eye = {t[1] + math.sin(yaw) * math.cos(pitch) * c.distance,
                          t[2] + math.sin(pitch) * c.distance,
                          t[3] + math.cos(yaw) * math.cos(pitch) * c.distance},
                   target = {t[1], t[2], t[3]}, fov = 45, far = 300})
  end
  return ctrl
end

-- UI ---------------------------------------------------------------------------

local function row(i) return ROW_Y + i * ROW_STEP end

local function adjust_star(i, delta)
  local s = E.settings.stars
  s[i] = math.max(500, s[i] + delta)
  for k = i + 1, 3 do s[k] = math.max(s[k], s[k - 1] + 500) end
  for k = i - 1, 1, -1 do s[k] = math.min(s[k], s[k + 1] - 500) end
  if s[1] < 500 then s[1] = 500; s[2] = math.max(s[2], 1000); s[3] = math.max(s[3], s[2] + 500) end
  E.settings.auto, E.dirty = false, true
end

local function draw_palette(h)
  tp.ui_rect(0, 48, LEFT_W, h - 48 - 34, 0.04, 0.1, 0.22, 0.72)
  tp.ui_text(14, 56, "MATERIALS  1-8", 1)
  for i, m in ipairs(Catalog.MATERIALS) do
    local y = 70 + (i - 1) * 34
    if i == E.material and E.shape ~= SHAPES then tp.ui_rect(6, y - 3, 224, 34, 1, 0.8, 0.2, 0.95) end
    tp.ui_rect(12, y + 2, 24, 24, m.color[1], m.color[2], m.color[3], 1)
    if tp.ui_button("mat_" .. m.id, 42, y, 182, 28, i .. " " .. m.name:upper()) then
      E.material, E.carry = i, nil
      if E.shape == SHAPES then E.shape = 1 end
      E.mode = "place"
    end
  end
  tp.ui_text(14, 348, "SHAPES  TAB", 1)
  for i = 1, SHAPES do
    local y = 362 + (i - 1) * 34
    local name = i == SHAPES and "GRUMP" or Catalog.SHAPES[i].name:upper()
    if i == E.shape then tp.ui_rect(6, y - 3, 224, 34, 1, 0.8, 0.2, 0.95) end
    if i == SHAPES then tp.ui_rect(12, y + 2, 24, 24, 0.25, 0.85, 0.1, 1) end
    if tp.ui_button("shape_" .. i, 42, y, 182, 28, name) then
      E.shape, E.carry, E.mode = i, nil, "place"
    end
  end
end

local function draw_settings(w)
  tp.ui_rect(RIGHT_X, 48, w - RIGHT_X, 580, 0.04, 0.1, 0.22, 0.72)
  local s = E.settings
  if tp.ui_button("name", PANEL_X, row(0), 206, 30, s.name:upper()) then
    new_name()
    E.dirty = true
  end
  local function stepper(id, i, label, minus, plus)
    if tp.ui_button(id .. "_minus", PANEL_X, row(i), 34, 30, "-") then minus() end
    tp.ui_text(PANEL_X + 44, row(i) + 8, label, 2)
    if tp.ui_button(id .. "_plus", PANEL_X + 172, row(i), 34, 30, "+") then plus() end
  end
  stepper("shots", 1, string.format("SHOTS %d", s.shots),
          function() s.shots = math.max(1, s.shots - 1); E.dirty = true; refresh_stars() end,
          function() s.shots = math.min(20, s.shots + 1); E.dirty = true; refresh_stars() end)
  for i = 1, 3 do
    stepper("star" .. i, 1 + i, string.rep("*", i) .. " " .. s.stars[i],
            function() adjust_star(i, -500) end, function() adjust_star(i, 500) end)
  end
  if tp.ui_button("auto", PANEL_X, row(5), 206, 30, s.auto and "AUTO STARS: ON" or "AUTO STARS: OFF") then
    s.auto = not s.auto
    refresh_stars()
  end
  local buttons = {
    {"load", "LOAD NEXT LEVEL", load_next}, {"new", "NEW LEVEL", function()
      before_edit(); E.carry = nil; respawn({})
      E.file, E.base, E.settings.auto = nil, {}, true
      new_name(); refresh_stars(); say("NEW LEVEL") end},
    {"save", "SAVE", save}, {"test", "PLAY TEST (P)", play_test}, {"reset", "RESET", reset_layout},
    {"undo", "UNDO (CTRL+Z)", undo},
    {"clear", confirm_clear > 0 and "CONFIRM CLEAR?" or "CLEAR ALL", clear_all},
    {"mode", E.mode == "place" and "MODE: PLACE (Q)" or "MODE: SELECT (Q)", function()
      E.mode = E.mode == "place" and "select" or "place"; E.carry = nil end},
    {"menu", "MENU (ESC)", leave},
  }
  for i, b in ipairs(buttons) do
    if tp.ui_button(b[1], PANEL_X, row(5 + i), 206, 30, b[2]) then b[3]() end
  end
end

-- Callbacks --------------------------------------------------------------------

return {
  start = function()
    E = Session.editor or new_state()
    Session.editor = E
    Session.playtest = nil
    tracker = Core.tracker()
    rig = Rig.new({pouch = POUCH, fort = FORT, look_ahead = 17})
    items, markers = {}, {}
    for _, item in ipairs(E.items) do spawn_item(item) end
    edit_pending = true
    refresh_stars()
    preview = tp.entity_create("editor preview")
    tp.component_set(preview, "transform", {position = {0, -10, 0}})
    tp.component_set(preview, "mesh", {mesh = "assets/wood.gltf", visible = false, casts_shadows = false})
    preview_mesh = "assets/wood.gltf"
    preview_marker = tp.entity_create("editor preview marker")
    tp.component_set(preview_marker, "transform", {position = {0, -10, 0}, scale = {0.14, 0.14, 0.14}})
    tp.component_set(preview_marker, "mesh", {mesh = "assets/marker_preview.gltf", visible = false,
                                              casts_shadows = false})
    select_marker = tp.entity_create("editor select marker")
    tp.component_set(select_marker, "transform", {position = {0, -10, 0}, scale = {0.16, 0.16, 0.16}})
    tp.component_set(select_marker, "mesh", {mesh = "assets/marker_select.gltf", visible = false,
                                             casts_shadows = false})
    tp.set_exposure(0.95)
    tp.set_environment("assets/day.hdr", 0.7)
    tp.set_sky({zenith = {0.03, 0.17, 1.0}, horizon = {0.42, 0.66, 1.15}, ground = {0.25, 0.5, 0.15},
                sun_direction = {0.55, 1, 0.45}, sun_color = {14, 12, 9}, sun_size = 2})
    tp.set_ssao(0)
    for _, name in ipairs({"exit", "drag", "ed_test"}) do held[name] = tp.action(name) > 0 end
    camera_input(0)
  end,

  fixed_update = function(dt)
    local speeds = {}
    for id, item in pairs(items) do
      local state = tp.physics_state(id)
      local speed = length(state.velocity)
      speeds[#speeds + 1] = speed
      local r = state.rotation
      local fallen = item.shape ~= "sphere" and item.kind ~= "target" and
        (math.abs(r[1]) > 0.35 or math.abs(r[3]) > 0.35)
      local half = item.kind == "target" and {GRUMP_RADIUS, GRUMP_RADIUS, GRUMP_RADIUS} or Core.half(item)
      local show = speed >= Core.CALM_SPEED or fallen
      tp.component_set(markers[id], "mesh", {visible = show})
      if show then
        tp.component_set(markers[id], "transform", {position = {state.position[1],
          Core.top(state.position, half, r, item.shape == "sphere") + 0.3, state.position[3]}})
      end
    end
    local stable = tracker:update(dt, speeds)
    if stable and edit_pending then
      E.snapshot = layout()
      edit_pending = false
    end
    message_time = math.max(0, message_time - dt)
    confirm_clear = math.max(0, confirm_clear - dt)
    confirm_leave = math.max(0, confirm_leave - dt)
  end,

  update = function(dt)
    dt = math.min(dt, 0.1)
    if edge("exit") then return leave() end
    local ctrl = camera_input(dt)
    if ctrl and edge("ed_zoom_in") then undo() end
    if not ctrl then held.ed_zoom_in = tp.action("ed_zoom_in") > 0 end
    for i = 1, #Catalog.MATERIALS do
      if edge("ed_" .. i) then
        E.material, E.carry, E.mode = i, nil, "place"
        if E.shape == SHAPES then E.shape = 1 end
      end
    end
    if edge("ed_shape") then E.shape, E.carry, E.mode = E.shape % SHAPES + 1, nil, "place" end
    if edge("ed_turn") then E.turned = not E.turned end
    if edge("ed_tip") then E.tipped = not E.tipped end
    if edge("ed_select") then E.mode, E.carry = E.mode == "place" and "select" or "place", nil end
    if edge("ed_delete") then delete_selected() end
    if edge("ed_grab") and E.selected then grab(E.selected) end
    if edge("ed_test") then return play_test() end
    update_hover()
    local x, y = tp.pointer()
    local down = tp.action("drag") > 0
    local pressed = down and not held.drag
    local released = not down and held.drag
    held.drag = down
    if pressed and not over_ui(x, y) then
      if E.mode == "place" or E.carry then
        place()
      else
        local ray = tp.screen_ray(x, y)
        local hit = tp.physics_raycast(ray.origin, ray.direction, 300)
        E.selected = hit and items[hit.id] and hit.id or nil
        press = E.selected and {x = x, y = y, id = E.selected} or nil
      end
    elseif down and press and not E.carry and math.abs(x - press.x) + math.abs(y - press.y) > 10 then
      grab(press.id)
      press.dragging = true
    elseif released and press then
      if press.dragging and E.carry then
        update_hover()
        if hover then place() else undo() end
      end
      press = nil
    end
  end,

  draw = function()
    local w, h = tp.viewport_size()
    local blocks, grumps = counts()
    tp.ui_rect(0, 0, w, 44, 0.05, 0.12, 0.25, 0.6)
    tp.ui_text(14, 13, "LEVEL EDITOR  " .. E.settings.name:upper(), 2)
    tp.ui_text(470, 13, string.format("BLOCKS %d  GRUMPS %d", #blocks, grumps), 2)
    local status
    if tracker.stable then
      status = "STABLE"
      tp.ui_rect(778, 6, 200, 32, 0.15, 0.6, 0.2, 0.85)
    elseif tracker.moving > 0 then
      status = string.format("UNSTABLE (%d)", tracker.moving)
      tp.ui_rect(778, 6, 200, 32, 0.8, 0.15, 0.1, 0.85)
    else
      status = "SETTLING"
      tp.ui_rect(778, 6, 200, 32, 0.75, 0.55, 0.1, 0.85)
    end
    tp.ui_text(790, 13, status, 2)
    tp.ui_text(1000, 16, E.dirty and "UNSAVED" or (E.file and "SAVED" or "NEW"), 1)
    if E.file then tp.ui_text(1000, 28, E.file:sub(8):upper(), 1) end
    draw_palette(h)
    draw_settings(w)
    local mode = E.carry and ("MOVING " .. E.carry.id:upper()) or
      (E.mode == "place" and ("PLACING " .. (E.shape == SHAPES and "GRUMP" or
        Catalog.MATERIALS[E.material].name:upper() .. " " .. Catalog.SHAPES[E.shape].name:upper()))
       or "SELECT: CLICK A BLOCK, DRAG TO MOVE, BKSP DELETE")
    tp.ui_text(LEFT_W + 12, 56, mode, 1)
    if message_time > 0 then
      tp.ui_rect(LEFT_W + 8, h - 70, RIGHT_X - LEFT_W - 16, 30, 0.04, 0.1, 0.22, 0.8)
      tp.ui_text(LEFT_W + 18, h - 61, message, 1)
    end
    tp.ui_rect(0, h - 32, w, 32, 0.05, 0.12, 0.25, 0.6)
    tp.ui_text(12, h - 27, "LMB PLACE   RMB/ARROWS ORBIT   WASD PAN   Z/X ZOOM   C RESET VIEW   " ..
      "V PLAYER VIEW   R TURN   F TIP   TAB SHAPE   1-8 MATERIAL", 1)
    tp.ui_text(12, h - 14, "Q SELECT MODE   CLICK SELECT, DRAG OR G MOVE   BKSP DELETE   CTRL+Z UNDO   " ..
      "P PLAY TEST   ESC MENU", 1)
  end,
}
