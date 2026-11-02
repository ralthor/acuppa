-- Level 2 data: three small towers of the newer materials. Knock the stone
-- boulder off its perch onto the grump below, crack the ice lintel under the
-- heavy metal block, and set off the crate on the bouncy rubber stand.
return {
  name = "Frozen Foundry",
  shots = 4,
  stars = {5000, 11000, 16000},
  kill_y = -1,
  pouch = {0, 3.3, 0},
  aim = {yaw = 0, pitch = 16, power = 0.42},
  fort_center = {0, 1.6, -18},
  ball = {radius = 0.3, density = 3500, friction = 0.5, restitution = 0.2},
  target_radius = 0.45,
  blocks = {
    {id = "left_pillar_a", material = "brick", position = {-3.9, 0.7, -18}, half_extents = {0.25, 0.7, 0.3}},
    {id = "left_pillar_b", material = "brick", position = {-2.1, 0.7, -18}, half_extents = {0.25, 0.7, 0.3}},
    {id = "left_lintel", material = "ice", position = {-3, 1.52, -18}, half_extents = {1.1, 0.12, 0.4}},
    {id = "left_weight", material = "metal", position = {-3, 1.99, -18}, half_extents = {0.35, 0.35, 0.35}},
    {id = "mid_pillar_a", material = "brick", position = {-1, 1, -18}, half_extents = {0.25, 1, 0.3}},
    {id = "mid_pillar_b", material = "brick", position = {1, 1, -18}, half_extents = {0.25, 1, 0.3}},
    {id = "mid_lintel", material = "wood", position = {0, 2.12, -18}, half_extents = {1.3, 0.12, 0.4}},
    {id = "mid_boulder", material = "stone", shape = "sphere", position = {-0.5, 2.69, -18}, radius = 0.45},
    {id = "mid_pane", material = "glass", position = {0.7, 2.54, -18}, half_extents = {0.3, 0.3, 0.05}},
    {id = "right_stand_a", material = "rubber", position = {2.6, 0.5, -18}, half_extents = {0.35, 0.5, 0.35}},
    {id = "right_stand_b", material = "rubber", position = {4.4, 0.5, -18}, half_extents = {0.35, 0.5, 0.35}},
    {id = "right_lintel", material = "metal", position = {3.5, 1.12, -18}, half_extents = {1.1, 0.12, 0.4}},
    {id = "right_crate", material = "tnt", position = {3.5, 1.59, -18}, half_extents = {0.35, 0.35, 0.35}},
    {id = "right_ice_a", material = "ice", position = {2.75, 1.54, -18}, half_extents = {0.3, 0.3, 0.3}},
    {id = "right_ice_b", material = "ice", position = {4.25, 1.54, -18}, half_extents = {0.3, 0.3, 0.3}},
  },
  targets = {
    {id = "grump_left_low", position = {-3, 0.45, -18}},
    {id = "grump_left_top", position = {-3, 2.79, -18}},
    {id = "grump_mid_low", position = {0, 0.45, -18}},
    {id = "grump_right_low", position = {3.5, 0.45, -18}},
    {id = "grump_right_top", position = {3.5, 2.39, -18}},
  },
}
