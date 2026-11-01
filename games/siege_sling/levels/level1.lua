-- Level 1 data: validated by Rules.validate_level. Positions are block
-- centers in meters; half_extents are box half sizes (the unit-box meshes
-- are scaled by them, so visuals match colliders). Blocks start exactly
-- touching so the fort rests without settling impacts.
return {
  name = "Grassy Hill",
  shots = 4,
  stars = {2500, 4000, 5500},
  kill_y = -1,
  pouch = {0, 3.3, 0},
  aim = {yaw = 0, pitch = 18, power = 0.42},
  fort_center = {0, 1.2, -21},
  ball = {radius = 0.3, density = 6000, friction = 0.5, restitution = 0.2},
  target_radius = 0.45,
  blocks = {
    -- Left tower: stone pillars, wood deck.
    {id = "left_pillar_a", material = "stone", position = {-4.2, 0.7, -21}, half_extents = {0.3, 0.7, 0.3}},
    {id = "left_pillar_b", material = "stone", position = {-2.6, 0.7, -21}, half_extents = {0.3, 0.7, 0.3}},
    {id = "left_deck", material = "wood", position = {-3.4, 1.55, -21}, half_extents = {1.15, 0.15, 0.4}},
    -- Center keep: wood posts, glass front, wood roof, stone cap.
    {id = "center_post_a", material = "wood", position = {-0.85, 0.9, -21.5}, half_extents = {0.25, 0.9, 0.25}},
    {id = "center_post_b", material = "wood", position = {0.85, 0.9, -21.5}, half_extents = {0.25, 0.9, 0.25}},
    {id = "center_pane", material = "glass", position = {0, 0.6, -20.9}, half_extents = {0.55, 0.6, 0.06}},
    {id = "center_roof", material = "wood", position = {0, 1.95, -21.5}, half_extents = {1.25, 0.15, 0.45}},
    {id = "center_cap", material = "stone", position = {0, 2.45, -21.5}, half_extents = {0.35, 0.35, 0.35}},
    -- Right tower: stone base, wood posts, glass pane, wood deck.
    {id = "right_base", material = "stone", position = {3.4, 0.3, -21}, half_extents = {1.0, 0.3, 0.45}},
    {id = "right_post_a", material = "wood", position = {2.75, 1.2, -21}, half_extents = {0.2, 0.6, 0.2}},
    {id = "right_post_b", material = "wood", position = {4.05, 1.2, -21}, half_extents = {0.2, 0.6, 0.2}},
    {id = "right_pane", material = "glass", position = {3.4, 1.1, -20.75}, half_extents = {0.45, 0.5, 0.06}},
    {id = "right_deck", material = "wood", position = {3.4, 1.92, -21}, half_extents = {0.95, 0.12, 0.4}},
  },
  targets = {
    {id = "grump_left", position = {-3.4, 2.15, -21}},
    {id = "grump_center", position = {0, 0.45, -21.5}},
    {id = "grump_right", position = {3.4, 2.49, -21}},
  },
}
