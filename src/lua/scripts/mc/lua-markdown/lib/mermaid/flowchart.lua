-- A flowchart: the nodes and the edges between them, drawn as boxes in
-- layers when they fit the screen and as a tree of lines when they do not.

local cfg = require("config")
local txt = require("text")
local glyphs = require("mermaid.glyphs")
local cv = require("mermaid.canvas")

local trim = txt.trim
local chars = txt.chars
local width = txt.char_count  -- a canvas counts cells, not columns

local ARROW_DOWN, ARROW_LEFT = glyphs.ARROW_DOWN, glyphs.ARROW_LEFT
local TEE, ELBOW, BAR, DASH, LOOP = glyphs.TEE, glyphs.ELBOW, glyphs.BAR, glyphs.DASH, glyphs.LOOP
local TRIANGLE = glyphs.TRIANGLE
local BOX_TL, BOX_TR, BOX_BL, BOX_BR = glyphs.BOX_TL, glyphs.BOX_TR, glyphs.BOX_BL, glyphs.BOX_BR
local ROUND_TL, ROUND_TR = glyphs.ROUND_TL, glyphs.ROUND_TR
local ROUND_BL, ROUND_BR = glyphs.ROUND_BL, glyphs.ROUND_BR
local DIAMOND_TL, DIAMOND_TR = glyphs.DIAMOND_TL, glyphs.DIAMOND_TR
local DIAMOND_BL, DIAMOND_BR = glyphs.DIAMOND_BL, glyphs.DIAMOND_BR
local DIAMOND = glyphs.DIAMOND
local BOX_H, BOX_V = glyphs.DASH, glyphs.BAR

local canvas_new = cv.new
local canvas_pen = cv.pen
local canvas_paint = cv.paint
local canvas_put = cv.put
local canvas_line = cv.line
local canvas_draw_lines = cv.draw_lines
local canvas_lines = cv.lines
local decision_edges = cv.decision_edges
local UP, DOWN, LEFT, RIGHT = cv.UP, cv.DOWN, cv.LEFT, cv.RIGHT

local M = {}


-- The label of a node and the shape it was written with.
local function node_text(label, shape)
    if shape == "{" then
        return "<" .. label .. ">"
    elseif shape == "(" then
        return "(" .. label .. ")"
    elseif shape == "((" then
        return "((" .. label .. "))"
    end
    return "[" .. label .. "]"
end

local function ensure_node(chart, id)
    if chart.nodes[id] == nil then
        chart.nodes[id] = { id = id, label = id, shape = "[", text = "[" .. id .. "]" }
        chart.order[#chart.order + 1] = id
    end
    return chart.nodes[id]
end

-- "A[Text]", "A(Text)", "A{Text}", "A((Text))" or a bare "A"
local function read_node(chart, s)
    local id, shape, label = s:match("^([%w_.-]+)%s*(%(%()(.-)%)%)$")
    if id == nil then
        id, shape, label = s:match("^([%w_.-]+)%s*([%[{(])(.-)[%]})]$")
    end
    if id == nil then
        id = s:match("^([%w_.-]+)$")
        if id == nil then
            return nil
        end
        return ensure_node(chart, id)
    end

    local node = ensure_node(chart, id)

    node.label = trim(label:gsub('^"(.*)"$', "%1"))
    node.shape = shape
    node.text = node_text(node.label, shape)
    return node
end

-- The edges of one line: "A --> B", "A -->|text| B", "A -- text --> B"
local function read_edge(chart, line)
    local left, arrow, label, right = line:match("^(.-)%s*([-=.]+[->]+)%s*|([^|]*)|%s*(.+)$")
    if left == nil then
        left, label, arrow, right = line:match("^(.-)%s+%-%-%s*([^-]-)%s*([-=.]+[->]+)%s*(.+)$")
    end
    if left == nil then
        left, arrow, right = line:match("^(.-)%s*([-=.]+[->]+)%s*(.+)$")
    end
    if left == nil then
        return false
    end

    local from = read_node(chart, trim(left))
    local to = read_node(chart, trim(right))
    if from == nil or to == nil then
        return false
    end
    chart.edges[#chart.edges + 1] = {
        from = from.id,
        to = to.id,
        label = label ~= nil and trim(label) ~= "" and trim(label) or nil,
        open = not arrow:find(">", 1, true),
    }
    return true
end

local function parse_flowchart(lines)
    local chart = { nodes = {}, order = {}, edges = {} }

    for _, raw in ipairs(lines) do
        local line = trim(raw:gsub("%%%%.*$", ""))

        if line == "" or line:match("^graph%s") or line:match("^flowchart%s")
            or line == "graph" or line == "flowchart" then
            -- the direction says where a real renderer would put the nodes
        elseif line:match("^subgraph") or line == "end" or line:match("^style%s")
            or line:match("^classDef%s") or line:match("^class%s") or line:match("^click%s") then
            -- not drawn, and not a reason to give up on the rest
        elseif not read_edge(chart, line) then
            if read_node(chart, line) == nil then
                return nil
            end
        end
    end
    if #chart.order == 0 then
        return nil
    end
    return chart
end

-- A flowchart as a tree: every node is shown under the one it comes from,
-- and a node already shown is named again with a loop mark.
local function draw_flowchart(chart)
    local out = {}
    local children = {}
    local incoming = {}

    for _, edge in ipairs(chart.edges) do
        children[edge.from] = children[edge.from] or {}
        table.insert(children[edge.from], edge)
        incoming[edge.to] = (incoming[edge.to] or 0) + 1
    end

    local shown = {}

    local function walk(id, prefix, last, edge)
        local node = chart.nodes[id]
        local line

        if prefix == "" then
            line = node.text
        else
            line = prefix .. (last and ELBOW or TEE) .. DASH
                .. (edge ~= nil and edge.open and DASH or ARROW_DOWN) .. " " .. node.text
        end
        if edge ~= nil and edge.label ~= nil then
            line = line .. "  " .. edge.label
        end
        if shown[id] then
            out[#out + 1] = line .. " " .. LOOP
            return
        end
        shown[id] = true
        out[#out + 1] = line

        local kids = children[id] or {}
        local next_prefix = prefix == "" and "" or (prefix .. (last and "  " or (BAR .. " ")))

        for i, child in ipairs(kids) do
            walk(child.to, next_prefix .. "  ", i == #kids, child)
        end
    end

    for _, id in ipairs(chart.order) do
        if (incoming[id] or 0) == 0 and not shown[id] then
            if #out > 0 then
                out[#out + 1] = ""
            end
            walk(id, "", true, nil)
        end
    end
    -- a graph that is all cycles has no root to start from
    for _, id in ipairs(chart.order) do
        if not shown[id] then
            if #out > 0 then
                out[#out + 1] = ""
            end
            walk(id, "", true, nil)
        end
    end
    return out
end

-- The layer of every node: one past the deepest layer of what comes into it.
local function layer_nodes(chart, children, incoming)
    local layer = {}
    local queue = {}

    for _, id in ipairs(chart.order) do
        if (incoming[id] or 0) == 0 then
            layer[id] = 1
            queue[#queue + 1] = id
        end
    end
    if #queue == 0 then
        -- every node is in a cycle: start where the text starts
        layer[chart.order[1]] = 1
        queue[1] = chart.order[1]
    end

    local guard = 0
    while #queue > 0 and guard < 10000 do
        local id = table.remove(queue, 1)

        guard = guard + 1
        for _, edge in ipairs(children[id] or {}) do
            local want = layer[id] + 1

            -- an edge that goes back would push its target on for ever: no
            -- node sits deeper than the number of nodes
            if want <= #chart.order and (layer[edge.to] == nil or layer[edge.to] < want) then
                layer[edge.to] = want
                queue[#queue + 1] = edge.to
            end
        end
    end
    for _, id in ipairs(chart.order) do
        layer[id] = layer[id] or 1
    end
    return layer
end

local function draw_flowchart_boxes(chart, width_limit)
    local children = {}
    local incoming = {}

    for _, edge in ipairs(chart.edges) do
        children[edge.from] = children[edge.from] or {}
        table.insert(children[edge.from], edge)
        incoming[edge.to] = (incoming[edge.to] or 0) + 1
    end

    local layer = layer_nodes(chart, children, incoming)
    local columns = {}
    local depth = 0

    for _, id in ipairs(chart.order) do
        local n = layer[id]

        columns[n] = columns[n] or {}
        table.insert(columns[n], id)
        depth = math.max(depth, n)
    end

    -- the widest label of a layer sets the width of its column, and the
    -- longest label of an edge the room between two layers
    local col_width = {}
    local gap = {}

    for n = 1, depth do
        col_width[n] = 0
        for _, id in ipairs(columns[n] or {}) do
            local extra =
                (chart.nodes[id].shape == "{" and cfg.DECISION_STYLE == "braille") and 6 or 4

            col_width[n] = math.max(col_width[n], width(chart.nodes[id].label) + extra)
        end
        gap[n] = 6
    end
    -- the room between two layers holds the label of an edge and a column
    -- for every line that turns there
    local fan = {}

    for _, edge in ipairs(chart.edges) do
        local n = layer[edge.from]

        if layer[edge.to] > n then
            fan[edge.from] = (fan[edge.from] or 0) + 1
            fan[n] = math.max(fan[n] or 1, fan[edge.from])
        end
    end
    for _, edge in ipairs(chart.edges) do
        local n = layer[edge.from]

        if edge.label ~= nil and n < depth then
            -- the label sits in the half of the room next to the box it
            -- points at
            gap[n] = math.max(gap[n], 2 * width(edge.label) + 6 + (fan[n] or 1) - 1)
        end
    end

    -- a line that jumps over a layer would run through the boxes standing
    -- in it; such a graph is drawn as a tree instead
    for _, edge in ipairs(chart.edges) do
        if layer[edge.to] - layer[edge.from] > 1 then
            return nil
        end
    end

    local col_x = {}
    local x = 1

    for n = 1, depth do
        col_x[n] = x
        x = x + col_width[n] + gap[n]
    end
    local total_width = x - gap[depth] - 1

    if width_limit ~= nil and total_width > width_limit then
        return nil
    end

    -- a box is three rows tall, a decision four; one row between them
    local row_y = {}
    local attach_y = {}

    local function node_height(id)
        return (chart.nodes[id].shape == "{" and cfg.DECISION_STYLE ~= "braille") and 4 or 3
    end

    for n = 1, depth do
        local y = 1

        for _, id in ipairs(columns[n] or {}) do
            row_y[id] = y
            attach_y[id] = y
                + ((chart.nodes[id].shape == "{" and cfg.DECISION_STYLE ~= "braille") and 2 or 1)
            y = y + node_height(id) + 1
        end
    end

    -- the first node of every layer is put on the same row as the others, so
    -- that a line between two of them runs straight
    local first_row = 0

    for n = 1, depth do
        local first = (columns[n] or {})[1]

        if first ~= nil then
            first_row = math.max(first_row, attach_y[first])
        end
    end
    for n = 1, depth do
        local first = (columns[n] or {})[1]

        if first ~= nil then
            local shift = first_row - attach_y[first]

            for _, id in ipairs(columns[n]) do
                row_y[id] = row_y[id] + shift
                attach_y[id] = attach_y[id] + shift
            end
        end
    end

    local canvas = canvas_new()

    for n = 1, depth do
        for _, id in ipairs(columns[n] or {}) do
            local node = chart.nodes[id]
            local w = col_width[n]
            local top = row_y[id]
            local pad = math.max(w - 2 - width(node.label), 0)
            local left = pad // 2
            local right = pad - left

            local body = (" "):rep(left) .. node.label .. (" "):rep(right)

            -- the shape the node was written with: a box, a rounded box, a
            -- circle or the diamond of a decision
            if node.shape == "{" and cfg.DECISION_STYLE == "braille" then
                local edge_top, edge_bottom = decision_edges(w - 2)

                canvas_put(canvas, top, col_x[n] + 1, edge_top)
                canvas_put(canvas, top + 1, col_x[n], DIAMOND .. body .. DIAMOND)
                canvas_put(canvas, top + 2, col_x[n] + 1, edge_bottom)
            elseif node.shape == "{" then
                canvas_put(canvas, top, col_x[n] + 2, ("_"):rep(w - 4))
                canvas_put(canvas, top + 1, col_x[n] + 1,
                           DIAMOND_TL .. (" "):rep(w - 4) .. DIAMOND_TR)
                canvas_put(canvas, top + 2, col_x[n], DIAMOND .. body .. DIAMOND)
                canvas_put(canvas, top + 3, col_x[n] + 1,
                           DIAMOND_BL .. ("_"):rep(w - 4) .. DIAMOND_BR)
            elseif node.shape == "((" then
                canvas_put(canvas, top + 1, col_x[n], "(" .. body .. ")")
            elseif node.shape == "(" then
                canvas_put(canvas, top, col_x[n], ROUND_TL .. BOX_H:rep(w - 2) .. ROUND_TR)
                canvas_put(canvas, top + 1, col_x[n], BOX_V .. body .. BOX_V)
                canvas_put(canvas, top + 2, col_x[n], ROUND_BL .. BOX_H:rep(w - 2) .. ROUND_BR)
            else
                canvas_put(canvas, top, col_x[n], BOX_TL .. BOX_H:rep(w - 2) .. BOX_TR)
                canvas_put(canvas, top + 1, col_x[n], BOX_V .. body .. BOX_V)
                canvas_put(canvas, top + 2, col_x[n], BOX_BL .. BOX_H:rep(w - 2) .. BOX_BR)
            end
        end
    end

    -- every edge leaves the right side of its box and enters the left side
    -- of the other one, turning in the room between the layers
    local turn = {}

    -- the line that has to travel farthest turns first, that is leftmost, so
    -- that a line never crosses one that turned before it
    local turn_order = {}

    do
        local leaving = {}

        for index, edge in ipairs(chart.edges) do
            if layer[edge.to] > layer[edge.from] then
                leaving[edge.from] = leaving[edge.from] or {}
                table.insert(leaving[edge.from], index)
            end
        end
        for _, list in pairs(leaving) do
            table.sort(list, function(a, b)
                local ea, eb = chart.edges[a], chart.edges[b]
                local da = math.abs(row_y[ea.to] - row_y[ea.from])
                local db = math.abs(row_y[eb.to] - row_y[eb.from])

                if da ~= db then
                    return da > db
                end
                return a < b
            end)
            for place, index in ipairs(list) do
                turn_order[index] = place
            end
        end
    end

    for edge_index, edge in ipairs(chart.edges) do
        local from_layer = layer[edge.from]
        local to_layer = layer[edge.to]
        local y1 = attach_y[edge.from]
        local y2 = attach_y[edge.to]
        local x1 = col_x[from_layer] + col_width[from_layer]
        local x2 = col_x[to_layer] - 1

        if to_layer > from_layer and x2 >= x1 then
            -- every line leaving a box turns in a column of its own, so that
            -- two lines never share a corner
            local mid = math.min(x1 + (x2 - x1) // 2 + (turn_order[edge_index] or 1) - 1, x2 - 2)

            turn[from_layer] = (turn[from_layer] or 0) + 1
            for i = x1, mid - 1 do
                canvas_line(canvas, y1, i, LEFT | RIGHT, edge_index)
            end
            if y1 == y2 then
                canvas_line(canvas, y1, mid, LEFT | RIGHT, edge_index)
            else
                local step = y1 < y2 and 1 or -1

                canvas_line(canvas, y1, mid, LEFT | (y1 < y2 and DOWN or UP), edge_index)
                for i = y1 + step, y2 - step, step do
                    canvas_line(canvas, i, mid, UP | DOWN, edge_index)
                end
                canvas_line(canvas, y2, mid, RIGHT | (y1 < y2 and UP or DOWN), edge_index)
            end
            for i = mid + 1, x2 - 1 do
                canvas_line(canvas, y2, i, LEFT | RIGHT, edge_index)
            end
            canvas_put(canvas, y2, x2, ARROW_DOWN)
            if edge.label ~= nil then
                -- over the line that runs into the box, where every edge has
                -- a row of its own
                local room = x2 - mid - 2

                if width(edge.label) <= room then
                    canvas_put(canvas, y2 - 1, x2 - width(edge.label) - 1, edge.label)
                else
                    canvas_put(canvas, y2 - 1, mid + 1,
                               table.concat(chars(edge.label), "", 1, math.max(room, 1)))
                end
            end
        else
            -- an edge that goes back or stays in its layer is named, not drawn
            local note = chart.nodes[edge.from].text .. " " .. ARROW_DOWN .. " "
                .. chart.nodes[edge.to].text .. (edge.label ~= nil and ("  " .. edge.label) or "")

            canvas_put(canvas, 1 + math.max(#(columns[1] or {}), 1) * 4 + #turn, 1, note)
        end
    end

    canvas_draw_lines(canvas)
    return canvas_lines(canvas)
end

M.parse = parse_flowchart
M.draw_tree = draw_flowchart
M.draw_boxes = draw_flowchart_boxes

return M
