-- Level 1 data: validated by Rules.validate_level. Positions are block
-- centers in meters; half_extents are box half sizes (the unit-box meshes
-- are scaled by them, so visuals match colliders). Blocks start exactly
-- touching so the fort rests without settling impacts.
return {
  name = "Grassy Hill",
  shots = 4,
  stars = {3000, 5000, 6500},
  kill_y = -1,
  pouch = {0, 3.3, 0},
  aim = {yaw = 0, pitch = 16, power = 0.42},
  fort_center = {0, 1.7, -19},
  ball = {radius = 0.3, density = 6000, friction = 0.5, restitution = 0.2},
  target_radius = 0.45,
  blocks = {
    -- Left tower: stone pillars, wood deck, wood posts around a glass pane, top deck.
    {id = "left_pillar_a", material = "stone", position = {-4.2, 0.7, -19}, half_extents = {0.3, 0.7, 0.3}},
    {id = "left_pillar_b", material = "stone", position = {-2.6, 0.7, -19}, half_extents = {0.3, 0.7, 0.3}},
    {id = "left_deck", material = "wood", position = {-3.4, 1.55, -19}, half_extents = {1.15, 0.15, 0.4}},
    {id = "left_post_a", material = "wood", position = {-4.1, 2.3, -19}, half_extents = {0.2, 0.6, 0.2}},
    {id = "left_post_b", material = "wood", position = {-2.7, 2.3, -19}, half_extents = {0.2, 0.6, 0.2}},
    {id = "left_pane", material = "glass", position = {-3.4, 2.3, -18.72}, half_extents = {0.48, 0.6, 0.06}},
    {id = "left_top", material = "wood", position = {-3.4, 3.02, -19}, half_extents = {0.95, 0.12, 0.4}},
    -- Center keep: wood posts, glass front, wood roof, stone upper floor and cap.
    {id = "center_post_a", material = "wood", position = {-0.85, 0.9, -19.5}, half_extents = {0.25, 0.9, 0.25}},
    {id = "center_post_b", material = "wood", position = {0.85, 0.9, -19.5}, half_extents = {0.25, 0.9, 0.25}},
    {id = "center_pane", material = "glass", position = {0, 0.6, -18.9}, half_extents = {0.55, 0.6, 0.06}},
    {id = "center_roof", material = "wood", position = {0, 1.95, -19.5}, half_extents = {1.25, 0.15, 0.45}},
    {id = "center_block_a", material = "stone", position = {-0.8, 2.45, -19.5}, half_extents = {0.35, 0.35, 0.35}},
    {id = "center_block_b", material = "stone", position = {0.8, 2.45, -19.5}, half_extents = {0.35, 0.35, 0.35}},
    {id = "center_slab", material = "stone", position = {0, 2.95, -19.5}, half_extents = {1.15, 0.15, 0.4}},
    {id = "center_cap", material = "stone", position = {0, 3.45, -19.5}, half_extents = {0.35, 0.35, 0.35}},
    -- Right tower: stone base, wood posts, glass pane, wood deck, stone pillars, plank.
    {id = "right_base", material = "stone", position = {3.4, 0.3, -19}, half_extents = {1.0, 0.3, 0.45}},
    {id = "right_post_a", material = "wood", position = {2.75, 1.2, -19}, half_extents = {0.2, 0.6, 0.2}},
    {id = "right_post_b", material = "wood", position = {4.05, 1.2, -19}, half_extents = {0.2, 0.6, 0.2}},
    {id = "right_pane", material = "glass", position = {3.4, 1.1, -18.75}, half_extents = {0.45, 0.5, 0.06}},
    {id = "right_deck", material = "wood", position = {3.4, 1.92, -19}, half_extents = {0.95, 0.12, 0.4}},
    {id = "right_pillar_a", material = "stone", position = {2.9, 2.54, -19}, half_extents = {0.2, 0.5, 0.2}},
    {id = "right_pillar_b", material = "stone", position = {3.9, 2.54, -19}, half_extents = {0.2, 0.5, 0.2}},
    {id = "right_top", material = "wood", position = {3.4, 3.16, -19}, half_extents = {0.8, 0.12, 0.4}},
  },
  targets = {
    {id = "grump_left", position = {-3.4, 3.59, -19}},
    {id = "grump_center", position = {0, 0.45, -19.5}},
    {id = "grump_right", position = {3.4, 3.73, -19}},
  },
}
