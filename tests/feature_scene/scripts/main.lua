local threepoint = require("threepoint")
assert(require("scripts.helper").project_local)
local cube

return {
  start = function()
    threepoint.set_exposure(1)
    assert(not pcall(threepoint.set_exposure, 0))
    assert(not pcall(threepoint.set_exposure, math.huge))
    assert(not pcall(threepoint.set_exposure, "bright"))
    cube = threepoint.spawn({mesh = "builtin:cube", rotation = {0.3, 0, 0}})
    local entity = threepoint.entity_create("component fixture")
    assert(threepoint.entity_exists(entity))
    assert(threepoint.entity_name(entity) == "component fixture")
    assert(#threepoint.entities() == 2)

    threepoint.component_set(entity, "transform", {position = {1, 2, 3}})
    threepoint.component_set(entity, "transform", {position = {2, 3, 4}})
    assert(threepoint.component_get(entity, "transform").position[1] == 2)
    threepoint.component_set(entity, "transform", {parent = cube})
    assert(threepoint.component_get(entity, "transform").parent == cube)

    threepoint.component_set(entity, "camera", {eye = {0, 1, 5}, active = true})
    threepoint.component_set(entity, "camera", {fov = 70})
    assert(threepoint.component_get(entity, "camera").fov == 70)
    threepoint.set_camera({eye = {0, 2, 5}})
    assert(threepoint.component_get(entity, "camera").eye[2] == 2)
    assert(threepoint.screen_ray(480, 270).origin[2] == 2)

    threepoint.component_set(entity, "light", {type = "point", intensity = 2})
    threepoint.component_set(entity, "light", {intensity = 3})
    assert(threepoint.component_get(entity, "light").intensity == 3)

    threepoint.component_set(entity, "mesh", {mesh = "builtin:cube", visible = false})
    threepoint.component_set(entity, "mesh", {visible = false})
    assert(not threepoint.component_get(entity, "mesh").visible)

    threepoint.component_set(entity, "script", {path = "scripts/main.lua", enabled = false})
    threepoint.component_set(entity, "script", {enabled = true})
    assert(threepoint.component_get(entity, "script").enabled)

    for _, kind in ipairs({"transform", "camera", "light", "mesh", "script"}) do
      assert(threepoint.component_remove(entity, kind))
      assert(threepoint.component_get(entity, kind) == nil)
    end
    threepoint.entity_destroy(entity)
    assert(not threepoint.entity_exists(entity))
    assert(#threepoint.entities() == 1)
  end,
  fixed_update = function(dt)
    assert(dt > 0)
  end,
  update = function(dt)
    assert(dt >= 0)
    local turn = threepoint.action("turn")
    threepoint.set_rotation(cube, {0.3, threepoint.time() * 0.9 + turn * 0.9, 0})
    local eye_x = threepoint.action("camera_right") + threepoint.action("pan")
    local eye_z = 4.5 + threepoint.action("select") + threepoint.action("jump")
    threepoint.set_camera({eye = {eye_x, 0, eye_z}})
    local x, y = threepoint.pointer()
    local ray = threepoint.screen_ray(x, y)
    assert(ray.direction[3] < 0)
  end,
  draw = function()
    assert(threepoint.entity_exists(cube))
  end,
}
