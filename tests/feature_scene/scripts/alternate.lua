local tp = require("threepoint")
local ticks = 0
return {
  start = function()
    assert(tp.scene_name() == "alternate")
    assert(tp.entity_find("missing") == nil)
    assert(tp.entity_find("anchor") == 1 and tp.entity_find("actor") == 2)
    assert(tp.component_get(2, "transform").parent == 1)
    local probe = tp.prefab_spawn("probe", {name = "custom probe",
                                                   transform = {position = {3, 0, 0}},
                                                   mesh = {visible = false}})
    assert(tp.entity_name(probe) == "custom probe")
    assert(tp.component_get(probe, "transform").position[1] == 3)
    tp.entity_destroy(probe)
    assert(not tp.entity_exists(probe))
    assert(not pcall(function() tp.prefab_spawn("missing") end))
    assert(not pcall(function() tp.prefab_spawn("probe", {mesh = {mesh = "missing"}}) end))
    assert(#tp.entities() == 2)
    assert(not pcall(function() tp.scene_switch("missing") end))
    _G.threepoint_alternate_loads = (_G.threepoint_alternate_loads or 0) + 1
  end,
  fixed_update = function(dt)
    assert(dt > 0)
    ticks = ticks + 1
  end,
  update = function(dt)
    assert(dt >= 0)
    assert(_G.threepoint_actor_starts == _G.threepoint_alternate_loads)
    assert(_G.threepoint_actor_fixed == (_G.threepoint_alternate_loads - 1) * 2 + ticks)
    assert(tp.component_get(tp.entity_find("actor"), "transform").position[1] == ticks)
    if ticks == 2 then
      assert(_G.threepoint_actor_updates == _G.threepoint_alternate_loads)
      assert(_G.threepoint_actor_draws == _G.threepoint_alternate_loads)
      if _G.threepoint_alternate_loads == 1 then tp.scene_reload()
      else tp.scene_switch("main") end
    end
  end,
}
