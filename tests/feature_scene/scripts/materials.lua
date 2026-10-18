local tp = require("acuppa")
return {
  start = function()
    assert(tp.entity_find("sun") and tp.component_get(tp.entity_find("sun"), "light"))
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt)
    assert(dt >= 0)
    local sun = tp.entity_find("sun")
    tp.component_set(sun, "light", {intensity = tp.action("turn") > 0 and 0 or
                                                (tp.scene_name() == "spheres" and 4 or 3)})
  end,
}
