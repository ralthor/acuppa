-- Draws the champion's NEAT network with the engine's screen UI rectangles.
-- Connections are rows of dots (the UI has no line primitive), green for
-- positive and red for negative weights; nodes brighten with their live value.
-- Every rectangle and text counts toward the engine's 256 UI items per frame,
-- so connections get only the items left in `budget`, strongest first.
local Panel = {}

local KIND_COLOR = {
  input = {0.3, 0.6, 1.0},
  hidden = {0.75, 0.45, 1.0},
  output = {1.0, 0.65, 0.2},
}
local NODE = 14

local function counts(net)
  local enabled = 0
  for _, link in ipairs(net.connections) do
    if link.enabled then enabled = enabled + 1 end
  end
  return #net.nodes, enabled
end

-- `values` maps node keys to their current activation. Returns the UI items
-- used and how many links were drawn.
function Panel.draw(tp, net, values, x, y, width, height, budget)
  local used = 0
  local function rect(...)
    used = used + 1
    tp.ui_rect(...)
  end
  local function text(...)
    used = used + 1
    tp.ui_text(...)
  end
  local node_count, link_count = counts(net)
  rect(x, y, width, height, 0.02, 0.03, 0.06, 0.88)
  text(x + 12, y + 10, string.format("CHAMPION NETWORK  %d NODES  %d LINKS",
                                     node_count, link_count), 1)

  local left, right = x + 70, x + width - 62
  local top, bottom = y + 34, y + height - 16
  local place = {}
  for _, node in ipairs(net.nodes) do
    place[node.key] = {left + node.x * (right - left), top + node.y * (bottom - top), node}
  end

  -- Nodes and labels are drawn last but reserved first, so links never crowd them out.
  local reserved = node_count
  for _, node in ipairs(net.nodes) do
    if node.label ~= "" then reserved = reserved + 1 end
  end
  local links = {}
  for _, link in ipairs(net.connections) do
    if link.enabled and place[link.from] and place[link.to] then links[#links + 1] = link end
  end
  table.sort(links, function(a, b)
    if math.abs(a.weight) ~= math.abs(b.weight) then return math.abs(a.weight) > math.abs(b.weight) end
    if a.from ~= b.from then return a.from < b.from end
    return a.to < b.to
  end)
  local spare = math.max(0, budget - used - reserved)
  local dots = #links > 0 and math.max(2, math.min(8, math.floor(spare / #links))) or 0
  local drawn = 0
  for _, link in ipairs(links) do
    if spare < dots then break end
    local a, b = place[link.from], place[link.to]
    local strength = math.min(1, math.abs(link.weight) / 3)
    local signal = math.min(1, math.abs(values[link.from] or 0))
    local alpha = 0.25 + 0.45 * strength + 0.3 * signal
    local size = math.abs(link.weight) > 1.5 and 5 or 3
    local r, g, bl = 0.3, 0.95, 0.45
    if link.weight < 0 then r, g, bl = 1.0, 0.35, 0.3 end
    for k = 1, dots do
      local t = k / (dots + 1)
      rect(a[1] + (b[1] - a[1]) * t - size / 2, a[2] + (b[2] - a[2]) * t - size / 2,
           size, size, r, g, bl, alpha)
    end
    spare = spare - dots
    drawn = drawn + 1
  end

  for _, node in ipairs(net.nodes) do
    local p = place[node.key]
    local base = KIND_COLOR[node.kind] or KIND_COLOR.hidden
    local level = 0.3 + 0.7 * math.min(1, math.abs(values[node.key] or 0))
    rect(p[1] - NODE / 2, p[2] - NODE / 2, NODE, NODE,
         base[1] * level, base[2] * level, base[3] * level, 1)
    if node.label ~= "" then
      if node.kind == "input" then text(p[1] - 66, p[2] - 4, node.label, 1)
      else text(p[1] + 12, p[2] - 4, node.label, 1) end
    end
  end
  return used, drawn
end

return Panel
