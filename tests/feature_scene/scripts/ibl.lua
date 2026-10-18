local tp = require("acuppa")
return {
  start = function()
    if tp.scene_name() == "ibl_missing" then
      tp.set_environment("assets/missing.hdr")
    elseif tp.scene_name() == "ibl" then
      tp.set_environment("assets/test_gradient.hdr", 1.0)
    else
      tp.set_environment(nil)
    end
  end,
  fixed_update = function(dt) assert(dt > 0) end,
  update = function(dt) assert(dt >= 0) end,
}
