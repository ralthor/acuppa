-- Siege Sling building catalog: named block shapes with default sizes, and
-- material display names and colors. Pure data and helpers with no engine
-- calls, shared by the level editor, menus, and tests.
local Catalog = {}

-- Ordered shape palette. Boxes give half extents (meters); spheres give a radius.
Catalog.SHAPES = {
  {id = "cube", name = "Cube", shape = "box", half_extents = {0.35, 0.35, 0.35}},
  {id = "small_cube", name = "Small cube", shape = "box", half_extents = {0.2, 0.2, 0.2}},
  {id = "plank", name = "Plank", shape = "box", half_extents = {0.6, 0.1, 0.35}},
  {id = "long_plank", name = "Long plank", shape = "box", half_extents = {1.1, 0.12, 0.4}},
  {id = "pillar", name = "Pillar", shape = "box", half_extents = {0.25, 0.7, 0.3}},
  {id = "slab", name = "Slab", shape = "box", half_extents = {0.6, 0.6, 0.06}},
  {id = "boulder", name = "Boulder", shape = "sphere", radius = 0.45},
}

-- Ordered materials: display name and an sRGB-ish UI color in 0..1.
Catalog.MATERIALS = {
  {id = "wood", name = "Wood", color = {0.85, 0.55, 0.25}},
  {id = "stone", name = "Stone", color = {0.62, 0.65, 0.7}},
  {id = "brick", name = "Brick", color = {0.86, 0.38, 0.22}},
  {id = "glass", name = "Glass", color = {0.7, 0.93, 1.0}},
  {id = "ice", name = "Ice", color = {0.75, 0.9, 1.0}},
  {id = "metal", name = "Metal", color = {0.72, 0.74, 0.78}},
  {id = "rubber", name = "Rubber", color = {0.18, 0.18, 0.2}},
  {id = "tnt", name = "TNT crate", color = {0.98, 0.5, 0.12}},
}

local shapes, materials = {}, {}
for _, entry in ipairs(Catalog.SHAPES) do shapes[entry.id] = entry end
for _, entry in ipairs(Catalog.MATERIALS) do materials[entry.id] = entry end

function Catalog.shape(id)
  local entry = shapes[id]
  if not entry then error("Catalog.shape: unknown shape " .. tostring(id), 2) end
  return entry
end

function Catalog.material(id)
  local entry = materials[id]
  if not entry then error("Catalog.material: unknown material " .. tostring(id), 2) end
  return entry
end

-- A new level block of a palette shape: copies sizes so callers may edit them.
function Catalog.block(shape_id, material_id, id, position)
  local shape = Catalog.shape(shape_id)
  Catalog.material(material_id)
  if type(id) ~= "string" or id == "" then error("Catalog.block: id must be a nonempty string", 2) end
  if type(position) ~= "table" or #position ~= 3 then
    error("Catalog.block: position must be an {x, y, z} table", 2)
  end
  local block = {id = id, material = material_id, position = {position[1], position[2], position[3]}}
  if shape.shape == "sphere" then
    block.shape, block.radius = "sphere", shape.radius
  else
    local h = shape.half_extents
    block.half_extents = {h[1], h[2], h[3]}
  end
  return block
end

return Catalog
