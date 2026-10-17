local tp = require("threepoint")
return {
  start = function()
    local id = tp.entity_find("imported triangle")
    assert(id and tp.component_get(id, "mesh").mesh == "assets/triangle.gltf")
    local spawned = tp.spawn({mesh = "assets/triangle.gltf", position = {3, 0, 0}})
    assert(tp.component_get(spawned, "mesh").mesh == "assets/triangle.gltf")
    tp.entity_destroy(spawned)
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt) assert(dt >= 0) end,
}
