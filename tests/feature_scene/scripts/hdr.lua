local tp = require("acuppa")
return {
  start = function()
    tp.set_exposure(tp.scene_name() == "hdr_dim" and 0.15 or 1.0)
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt) assert(dt >= 0) end,
}
