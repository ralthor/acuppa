local tp = require("threepoint")
local floor, ball, impact
local played = false

return {
  start = function()
    floor, ball = tp.entity_find("floor"), tp.entity_find("ball")
    tp.sound_set_listener({0, 2.5, 8}, {0, -0.2, -1}, {0, 1, 0})
    assert(tp.sound_state(1) == nil)
    assert(not pcall(tp.sound_play, "../outside.wav"))
    assert(not pcall(tp.sound_play, "assets/contact.wav", {volume = -1}))
    assert(not pcall(tp.sound_play, "assets/contact.wav", {pitch = 0}))
    assert(not pcall(tp.sound_play, "assets/contact.wav", {position = {0, 0, math.huge}}))
    assert(not pcall(tp.sound_play, "assets/contact.wav", {unknown = true}))
    assert(not pcall(tp.sound_set_listener, {0, 0, 0}, {0, 0, 0}, {0, 1, 0}))
    assert(not pcall(tp.sound_stop, 999))
    local quiet = tp.sound_play("assets/contact.wav", {volume = 0, spatial = false, loop = true})
    assert(tp.sound_state(quiet).loop and not tp.sound_state(quiet).spatial)
    tp.sound_set_pitch(quiet, 1.4)
    assert(math.abs(tp.sound_state(quiet).pitch - 1.4) < 0.001)
    assert(not pcall(tp.sound_set_pitch, quiet, math.huge))
    tp.sound_stop(quiet)
  end,
  fixed_update = function()
    for _, contact in ipairs(tp.physics_contacts()) do
      if not played and contact.began and not contact.trigger and
         (contact.first == floor or contact.second == floor) and
         (contact.first == ball or contact.second == ball) then
        assert(contact.closing_speed > 1 and math.abs(contact.normal[2]) > 0.9)
        impact = tp.sound_play("assets/contact.wav", {position = {-1, 0, 0}, volume = 0.75})
        assert(tp.sound_state(impact).spatial)
        tp.sound_set_position(impact, {-0.5, 0, 0})
        tp.sound_set_volume(impact, 0.5)
        local state = tp.sound_state(impact)
        assert(state.volume == 0.5 and state.position[1] == -0.5)
        assert(not pcall(tp.sound_set_volume, impact, math.huge))
        assert(not pcall(tp.sound_set_position, impact, {0, 0, 0/0}))
        played = true
      end
    end
    if tp.time() > 1.4 and impact then assert(not tp.sound_state(impact).playing) end
    if tp.time() > 1.5 and impact then
      tp.sound_stop(impact)
      assert(tp.sound_state(impact) == nil)
      impact = nil
    end
    if tp.time() > 2 then assert(played) end
  end,
  update = function() end,
  draw = function()
    tp.ui_text(24, 24, played and "CONTACT SOUND PLAYED" or "WAIT FOR IMPACT", 2)
  end,
}
