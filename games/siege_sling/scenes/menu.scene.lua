-- Title and level-select backdrop: the meadow, hills, trees, and a small
-- fort under the daytime sky. scripts/menu.lua draws the menus over it.
local entities = {
  {name = "grass", mesh = {mesh = "assets/grass.gltf"},
   transform = {position = {0, 0, -15}, scale = {150, 1, 150}}},
  {name = "hill", mesh = {mesh = "assets/hill.gltf", casts_shadows = false},
   transform = {position = {-7, 0, -6}}},
  {name = "sun", transform = {position = {0, 0, -11}},
   light = {type = "directional", direction = {-0.55, -1, -0.45},
            color = {1, 0.95, 0.85}, intensity = 2.6, shadows = true, shadow_range = 16}},
  {name = "view", camera = {eye = {0, 2.2, 9}, target = {2, 3.2, -18}, fov = 45,
                            far = 250, active = true}},
}
for i, hill in ipairs({{-50, -95, 1.6}, {42, -105, 2.08}, {100, -70, 1.44}, {5, -140, 2.56},
                       {-100, -60, 1.76}}) do
  entities[#entities + 1] = {name = "far hill " .. i,
    mesh = {mesh = "assets/far_hill.gltf", casts_shadows = false},
    transform = {position = {hill[1], 0, hill[2]}, scale = {3, hill[3], 3}}}
end
for i, tree in ipairs({{-14, -36, 1.0}, {-22, -29, 1.2}, {16, -39, 1.1}, {25, -31, 0.9},
                       {-8, -50, 1.0}, {11, -55, 1.4}, {30, -48, 1.2}}) do
  local s = tree[3]
  entities[#entities + 1] = {name = "tree " .. i .. " trunk",
    mesh = {mesh = "assets/trunk.gltf", casts_shadows = false},
    transform = {position = {tree[1], s, tree[2]}, scale = {0.18 * s, s, 0.18 * s}}}
  entities[#entities + 1] = {name = "tree " .. i .. " crown",
    mesh = {mesh = i % 2 == 0 and "assets/foliage_light.gltf" or "assets/foliage.gltf",
            casts_shadows = false},
    transform = {position = {tree[1], 2.9 * s, tree[2]}, scale = {1.3 * s, 1.6 * s, 1.3 * s}}}
end
-- A little fort on the right, built from the game's own block meshes.
for i, block in ipairs({
    {"stone", {6.2, 0.7, -4}, {0.3, 0.7, 0.35}}, {"stone", {8.4, 0.7, -4}, {0.3, 0.7, 0.35}},
    {"wood", {7.3, 1.52, -4}, {1.4, 0.12, 0.4}}, {"brick", {7.3, 2.04, -4}, {0.4, 0.4, 0.4}},
    {"ice", {6.4, 1.94, -4}, {0.3, 0.3, 0.3}}, {"metal", {8.2, 1.89, -4}, {0.25, 0.25, 0.25}},
    {"tnt", {7.3, 0.35, -4}, {0.35, 0.35, 0.35}}}) do
  entities[#entities + 1] = {name = "fort " .. i, mesh = {mesh = "assets/" .. block[1] .. ".gltf"},
    transform = {position = block[2], scale = block[3]}}
end
entities[#entities + 1] = {name = "grump", mesh = {mesh = "assets/grump.gltf"},
  transform = {position = {7.3, 2.9, -4}, scale = {0.45, 0.45, 0.45}}}
entities[#entities + 1] = {name = "boulder", mesh = {mesh = "assets/boulder_rubber.gltf"},
  transform = {position = {4.6, 0.45, -3}, scale = {0.45, 0.45, 0.45}}}
return {entities = entities, script = "scripts/menu.lua"}
