local tp = require("threepoint")
return {
  start = function(id)
    assert(tp.entity_name(id) == "actor")
    _G.threepoint_actor_starts = (_G.threepoint_actor_starts or 0) + 1
  end,
  fixed_update = function(id, dt)
    assert(dt > 0)
    local transform = tp.component_get(id, "transform")
    tp.component_set(id, "transform", {position = {transform.position[1] + 1, 0, 0}})
    _G.threepoint_actor_fixed = (_G.threepoint_actor_fixed or 0) + 1
  end,
  update = function(id, dt)
    assert(id == tp.entity_find("actor") and dt >= 0)
    _G.threepoint_actor_updates = (_G.threepoint_actor_updates or 0) + 1
  end,
  draw = function(id)
    assert(tp.entity_exists(id))
    _G.threepoint_actor_draws = (_G.threepoint_actor_draws or 0) + 1
  end,
}
