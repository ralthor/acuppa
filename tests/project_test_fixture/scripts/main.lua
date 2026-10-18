local tp = require("threepoint")
return {
  start = function() assert(tp.entity_find("cube")) end,
  fixed_update = function(dt) assert(dt == 1 / 60) end,
  update = function(dt) assert(dt >= 0) end,
  draw = function() assert(tp.entity_find("cube")) end,
}
