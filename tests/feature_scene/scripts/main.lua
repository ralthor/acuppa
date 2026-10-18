local acuppa = require("acuppa")
assert(require("scripts.helper").project_local)
local cube

return {
  start = function()
    acuppa.set_exposure(1)
    assert(not pcall(acuppa.set_exposure, 0))
    assert(not pcall(acuppa.set_exposure, math.huge))
    assert(not pcall(acuppa.set_exposure, "bright"))
    acuppa.set_ssao(2, 0.4, 1.5)
    assert(not pcall(acuppa.set_ssao, -1))
    assert(not pcall(acuppa.set_ssao, 4))
    assert(not pcall(acuppa.set_ssao, 1.5))
    assert(not pcall(acuppa.set_ssao, 2, 0))
    assert(not pcall(acuppa.set_ssao, 2, math.huge))
    assert(not pcall(acuppa.set_ssao, 2, 0.4, -1))
    acuppa.set_ssao(0)
    acuppa.set_environment("assets/test_gradient.hdr", 0)
    assert(not pcall(acuppa.set_environment, "assets/missing.hdr"))
    assert(not pcall(acuppa.set_environment, "../outside.hdr"))
    assert(not pcall(acuppa.set_environment, "assets/test_gradient.hdr", math.huge))
    acuppa.set_environment(nil)
    assert(not pcall(acuppa.spawn, {mesh = "builtin:cube", casts_shadows = 1}))
    cube = acuppa.spawn({mesh = "builtin:cube", rotation = {0.3, 0, 0},
                             casts_shadows = false})
    assert(not acuppa.component_get(cube, "mesh").casts_shadows)
    local entity = acuppa.entity_create("component fixture")
    assert(acuppa.entity_exists(entity))
    assert(acuppa.entity_name(entity) == "component fixture")
    assert(#acuppa.entities() == 2)

    acuppa.component_set(entity, "transform", {position = {1, 2, 3}})
    acuppa.component_set(entity, "transform", {position = {2, 3, 4}})
    assert(acuppa.component_get(entity, "transform").position[1] == 2)
    acuppa.component_set(entity, "transform", {parent = cube})
    assert(acuppa.component_get(entity, "transform").parent == cube)

    acuppa.component_set(entity, "camera", {eye = {0, 1, 5}, active = true})
    acuppa.component_set(entity, "camera", {fov = 70})
    assert(acuppa.component_get(entity, "camera").fov == 70)
    acuppa.set_camera({eye = {0, 2, 5}})
    assert(acuppa.component_get(entity, "camera").eye[2] == 2)
    assert(acuppa.screen_ray(480, 270).origin[2] == 2)

    acuppa.component_set(entity, "light", {type = "point", intensity = 2})
    acuppa.component_set(entity, "light", {intensity = 3})
    assert(acuppa.component_get(entity, "light").intensity == 3)
    acuppa.component_set(entity, "light", {shadows = true, shadow_range = 12})
    assert(acuppa.component_get(entity, "light").shadows)
    assert(acuppa.component_get(entity, "light").shadow_range == 12)
    acuppa.component_set(entity, "light", {shadows = false})

    acuppa.component_set(entity, "mesh", {mesh = "builtin:cube", visible = false,
                                               casts_shadows = false})
    acuppa.component_set(entity, "mesh", {visible = false})
    assert(not acuppa.component_get(entity, "mesh").visible)
    assert(not acuppa.component_get(entity, "mesh").casts_shadows)
    assert(not pcall(acuppa.component_set, entity, "mesh", {casts_shadows = 1}))
    assert(not acuppa.component_get(entity, "mesh").casts_shadows)

    acuppa.component_set(entity, "script", {path = "scripts/main.lua", enabled = false})
    acuppa.component_set(entity, "script", {enabled = true})
    assert(acuppa.component_get(entity, "script").enabled)

    for _, kind in ipairs({"transform", "camera", "light", "mesh", "script"}) do
      assert(acuppa.component_remove(entity, kind))
      assert(acuppa.component_get(entity, kind) == nil)
    end
    acuppa.entity_destroy(entity)
    assert(not acuppa.entity_exists(entity))
    assert(#acuppa.entities() == 1)
  end,
  fixed_update = function(dt)
    assert(dt > 0)
  end,
  update = function(dt)
    assert(dt >= 0)
    local turn = acuppa.action("turn")
    acuppa.set_rotation(cube, {0.3, acuppa.time() * 0.9 + turn * 0.9, 0})
    local eye_x = acuppa.action("camera_right") + acuppa.action("pan")
    local eye_z = 4.5 + acuppa.action("select") + acuppa.action("jump")
    acuppa.set_camera({eye = {eye_x, 0, eye_z}})
    local x, y = acuppa.pointer()
    local ray = acuppa.screen_ray(x, y)
    assert(ray.direction[3] < 0)
  end,
  draw = function()
    assert(acuppa.entity_exists(cube))
  end,
}
