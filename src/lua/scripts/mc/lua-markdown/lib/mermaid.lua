-- Mermaid diagrams as text: a flowchart becomes a tree of boxes, a sequence
-- diagram the lifelines of its participants.  What the parser does not know
-- is left to the caller, which shows the block as code.

local M = {}

local ARROW_DOWN = "\u{25B6}"  -- the head of an arrow going right
local TEE = "\u{251C}"
local ELBOW = "\u{2514}"
local BAR = "\u{2502}"
local DASH = "\u{2500}"
local ARROW_LEFT = "\u{25C0}"
local LOOP = "\u{21BA}"

------------------------------------------------------------------------
-- Text helpers, kept here so that the module stands on its own.

local function trim(s)
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function chars(s)
    local out = {}
    local i, n = 1, #s
    while i <= n do
        local b = s:byte(i)
        local len = 1
        if b >= 0xF0 then
            len = 4
        elseif b >= 0xE0 then
            len = 3
        elseif b >= 0xC0 then
            len = 2
        end
        out[#out + 1] = s:sub(i, i + len - 1)
        i = i + len
    end
    return out
end

local function width(s)
    return #chars(s)
end

------------------------------------------------------------------------
-- Flowchart.

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

-- Boxes on a canvas: every node is drawn once, in the layer its longest
-- path from a root puts it in, and the edges are drawn between the layers.

local BOX_TL, BOX_TR, BOX_BL, BOX_BR = "\u{250C}", "\u{2510}", "\u{2514}", "\u{2518}"
local BOX_H, BOX_V = "\u{2500}", "\u{2502}"

local function canvas_new()
    return { rows = {}, mask = {}, width = 0 }
end

local function canvas_put(canvas, y, x, text)
    local row = canvas.rows[y] or {}
    local i = x

    for _, ch in ipairs(chars(text)) do
        row[i] = ch
        i = i + 1
    end
    canvas.rows[y] = row
    canvas.width = math.max(canvas.width, i - 1)
end

-- Lines are kept as the directions they leave a cell in, and the character
-- is chosen once all of them are known.
local UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8

local LINE_GLYPH = {
    [LEFT + RIGHT] = "\u{2500}", [LEFT] = "\u{2500}", [RIGHT] = "\u{2500}",
    [UP + DOWN] = "\u{2502}", [UP] = "\u{2502}", [DOWN] = "\u{2502}",
    [DOWN + RIGHT] = "\u{250C}", [DOWN + LEFT] = "\u{2510}",
    [UP + RIGHT] = "\u{2514}", [UP + LEFT] = "\u{2518}",
    [UP + DOWN + RIGHT] = "\u{251C}", [UP + DOWN + LEFT] = "\u{2524}",
    [DOWN + LEFT + RIGHT] = "\u{252C}", [UP + LEFT + RIGHT] = "\u{2534}",
    [UP + DOWN + LEFT + RIGHT] = "\u{253C}",
}

local function canvas_line(canvas, y, x, dirs)
    canvas.mask[y] = canvas.mask[y] or {}
    canvas.mask[y][x] = (canvas.mask[y][x] or 0) | dirs
    canvas.width = math.max(canvas.width, x)
end

local function canvas_draw_lines(canvas)
    for y, row in pairs(canvas.mask) do
        for x, dirs in pairs(row) do
            local cell = canvas.rows[y] ~= nil and canvas.rows[y][x] or nil

            if cell == nil or cell == " " then
                canvas_put(canvas, y, x, LINE_GLYPH[dirs] or "\u{253C}")
            end
        end
    end
end

local function canvas_lines(canvas)
    local out = {}
    local last = 0

    for y in pairs(canvas.rows) do
        last = math.max(last, y)
    end
    for y = 1, last do
        local row = canvas.rows[y] or {}
        local line = {}

        for x = 1, canvas.width do
            line[x] = row[x] or " "
        end
        out[y] = (table.concat(line):gsub("%s+$", ""))
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
            col_width[n] = math.max(col_width[n], width(chart.nodes[id].label) + 4)
        end
        gap[n] = 6
    end
    for _, edge in ipairs(chart.edges) do
        local n = layer[edge.from]

        if edge.label ~= nil and n < depth then
            -- the label sits in the half of the room next to the box it
            -- points at
            gap[n] = math.max(gap[n], 2 * width(edge.label) + 6)
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

    -- three rows per box and one between them
    local row_y = {}
    local y = {}

    for n = 1, depth do
        y[n] = 1
        for i, id in ipairs(columns[n] or {}) do
            row_y[id] = 1 + (i - 1) * 4
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

            canvas_put(canvas, top, col_x[n], BOX_TL .. BOX_H:rep(w - 2) .. BOX_TR)
            canvas_put(canvas, top + 1, col_x[n],
                       BOX_V .. (" "):rep(left) .. node.label .. (" "):rep(right) .. BOX_V)
            canvas_put(canvas, top + 2, col_x[n], BOX_BL .. BOX_H:rep(w - 2) .. BOX_BR)
        end
    end

    -- every edge leaves the right side of its box and enters the left side
    -- of the other one, turning in the room between the layers
    local turn = {}

    for _, edge in ipairs(chart.edges) do
        local from_layer = layer[edge.from]
        local to_layer = layer[edge.to]
        local y1 = row_y[edge.from] + 1
        local y2 = row_y[edge.to] + 1
        local x1 = col_x[from_layer] + col_width[from_layer]
        local x2 = col_x[to_layer] - 1

        if to_layer > from_layer and x2 >= x1 then
            local mid = x1 + (x2 - x1) // 2

            turn[from_layer] = (turn[from_layer] or 0) + 1
            for i = x1, mid - 1 do
                canvas_line(canvas, y1, i, LEFT | RIGHT)
            end
            if y1 == y2 then
                canvas_line(canvas, y1, mid, LEFT | RIGHT)
            else
                local step = y1 < y2 and 1 or -1

                canvas_line(canvas, y1, mid, LEFT | (y1 < y2 and DOWN or UP))
                for i = y1 + step, y2 - step, step do
                    canvas_line(canvas, i, mid, UP | DOWN)
                end
                canvas_line(canvas, y2, mid, RIGHT | (y1 < y2 and UP or DOWN))
            end
            for i = mid + 1, x2 - 1 do
                canvas_line(canvas, y2, i, LEFT | RIGHT)
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

------------------------------------------------------------------------
-- Class diagram.

local TRIANGLE = "\u{25B3}"  -- the head of an inheritance arrow

local function class_of(model, name)
    name = trim(name)
    if model.classes[name] == nil then
        model.classes[name] = { name = name, attrs = {}, methods = {} }
        model.order[#model.order + 1] = name
    end
    return model.classes[name]
end

-- "+int age" is a field, "+mate()" a method
local function class_member(cls, text)
    text = trim(text)
    if text == "" then
        return
    end
    if text:find("%(") then
        cls.methods[#cls.methods + 1] = text
    else
        cls.attrs[#cls.attrs + 1] = text
    end
end

local function parse_class(lines)
    local model = { classes = {}, order = {}, relations = {} }
    local open = nil

    for _, raw in ipairs(lines) do
        local line = trim(raw:gsub("%%%%.*$", ""))
        local name, body = line:match("^class%s+([%w_]+)%s*{(.*)$")

        if line == "" or line == "classDiagram" or line:match("^classDiagram%-") then
            -- the header
        elseif open ~= nil then
            if line == "}" then
                open = nil
            else
                class_member(open, line)
            end
        elseif name ~= nil then
            open = class_of(model, name)
            class_member(open, body)
        elseif line:match("^class%s+[%w_]+$") then
            class_of(model, line:match("^class%s+([%w_]+)$"))
        elseif line:match("^<<") then
            -- a stereotype is not drawn
        else
            local from, arrow, to, label =
                line:match("^([%w_]+)%s*([<>|o*%.%-]+)%s*([%w_]+)%s*:%s*(.*)$")

            if from == nil then
                from, arrow, to = line:match("^([%w_]+)%s*([<>|o*%.%-]+)%s*([%w_]+)%s*$")
            end
            if from ~= nil then
                class_of(model, from)
                class_of(model, to)
                model.relations[#model.relations + 1] = {
                    from = from,
                    to = to,
                    label = label ~= nil and trim(label) ~= "" and trim(label) or nil,
                    -- "Animal <|-- Duck": the one on the right inherits the
                    -- one on the left
                    inherits = arrow:find("<|", 1, true) ~= nil or arrow:find("|>", 1, true) ~= nil,
                    reversed = arrow:find("|>", 1, true) ~= nil,
                }
            else
                local owner, member = line:match("^([%w_]+)%s*:%s*(.+)$")

                if owner == nil then
                    return nil
                end
                class_member(class_of(model, owner), member)
            end
        end
    end
    if #model.order == 0 then
        return nil
    end
    return model
end

-- Boxes of three parts, the children under the class they come from.
local function draw_class(model, width_limit)
    local parents = {}
    local children = {}

    for _, rel in ipairs(model.relations) do
        if rel.inherits then
            local parent = rel.reversed and rel.to or rel.from
            local child = rel.reversed and rel.from or rel.to

            parents[child] = parent
            children[parent] = children[parent] or {}
            table.insert(children[parent], child)
        end
    end

    -- the depth of a class is one past the depth of the class it comes from
    local depth = {}

    local function class_depth(name, guard)
        if depth[name] ~= nil then
            return depth[name]
        end
        if parents[name] == nil or guard > #model.order then
            depth[name] = 1
        else
            depth[name] = class_depth(parents[name], guard + 1) + 1
        end
        return depth[name]
    end

    local rows = {}
    local levels = 0

    for _, name in ipairs(model.order) do
        local d = class_depth(name, 0)

        rows[d] = rows[d] or {}
        table.insert(rows[d], name)
        levels = math.max(levels, d)
    end

    -- the size of every box
    local box_w = {}
    local box_h = {}

    for _, name in ipairs(model.order) do
        local cls = model.classes[name]
        local w = width(name)

        for _, list in ipairs({ cls.attrs, cls.methods }) do
            for _, text in ipairs(list) do
                w = math.max(w, width(text))
            end
        end
        box_w[name] = w + 4
        box_h[name] = 3 + #cls.attrs + #cls.methods
            + (#cls.attrs > 0 and 1 or 0) + (#cls.methods > 0 and 1 or 0)
    end

    local gap = 3
    local canvas = canvas_new()
    local x_of = {}
    local y_of = {}
    local y = 1

    for level = 1, levels do
        local x = 1
        local tallest = 0

        for _, name in ipairs(rows[level] or {}) do
            x_of[name] = x
            y_of[name] = y
            x = x + box_w[name] + gap
            tallest = math.max(tallest, box_h[name])
        end
        if width_limit ~= nil and x - gap - 1 > width_limit then
            return nil
        end
        y = y + tallest + 3
    end

    for _, name in ipairs(model.order) do
        local cls = model.classes[name]
        local w = box_w[name]
        local top = y_of[name]
        local left = x_of[name]
        local at = top

        local function put_row(text, centered)
            local pad = math.max(w - 2 - width(text), 0)
            local before = centered and pad // 2 or 1

            canvas_put(canvas, at, left,
                       BOX_V .. (" "):rep(before) .. text
                           .. (" "):rep(math.max(pad - before, 0)) .. BOX_V)
            at = at + 1
        end

        local function rule(left_ch, right_ch)
            canvas_put(canvas, at, left, left_ch .. BOX_H:rep(w - 2) .. right_ch)
            at = at + 1
        end

        rule(BOX_TL, BOX_TR)
        put_row(name, true)
        if #cls.attrs > 0 then
            rule("\u{251C}", "\u{2524}")
            for _, text in ipairs(cls.attrs) do
                put_row(text, false)
            end
        end
        if #cls.methods > 0 then
            rule("\u{251C}", "\u{2524}")
            for _, text in ipairs(cls.methods) do
                put_row(text, false)
            end
        end
        rule(BOX_BL, BOX_BR)
    end

    -- an inheritance arrow leaves the top of the child and enters the bottom
    -- of the class it comes from
    for parent, kids in pairs(children) do
        local bus = y_of[kids[1]] - 2
        local px = x_of[parent] + box_w[parent] // 2
        local py = y_of[parent] + box_h[parent] - 1

        for _, child in ipairs(kids) do
            local cx = x_of[child] + box_w[child] // 2
            local cy = y_of[child]

            for i = bus + 1, cy - 1 do
                canvas_line(canvas, i, cx, UP | DOWN)
            end
            canvas_line(canvas, bus, cx, DOWN | (cx < px and RIGHT or LEFT))
            local lo = math.min(cx, px)
            local hi = math.max(cx, px)

            for i = lo + 1, hi - 1 do
                canvas_line(canvas, bus, i, LEFT | RIGHT)
            end
        end
        canvas_line(canvas, bus, px, UP | LEFT | RIGHT)
        for i = py + 1, bus - 1 do
            canvas_line(canvas, i, px, UP | DOWN)
        end
        canvas_put(canvas, py + 1, px, TRIANGLE)
    end

    canvas_draw_lines(canvas)

    local out = canvas_lines(canvas)

    -- what is not inheritance is named under the diagram
    for _, rel in ipairs(model.relations) do
        if not rel.inherits then
            out[#out + 1] = rel.from .. " " .. ARROW_DOWN .. " " .. rel.to
                .. (rel.label ~= nil and ("  " .. rel.label) or "")
        end
    end
    return out
end

------------------------------------------------------------------------
-- Sequence diagram.

local function parse_sequence(lines)
    local seq = { names = {}, order = {}, steps = {} }

    local function participant(name)
        name = trim(name)
        if name == "" then
            return nil
        end
        if seq.names[name] == nil then
            seq.names[name] = name
            seq.order[#seq.order + 1] = name
        end
        return name
    end

    for _, raw in ipairs(lines) do
        local line = trim(raw:gsub("%%%%.*$", ""))
        local id, label = line:match("^[%a]+%s+([%w_]+)%s+as%s+(.+)$")

        if not line:match("^participants?%s") and not line:match("^actors?%s") then
            id, label = nil, nil
        end

        if line == "" or line == "sequenceDiagram" then
            -- the header
        elseif id ~= nil then
            participant(trim(label))
            seq.names[id] = trim(label)
        elseif line:match("^participants?%s+") or line:match("^actors?%s+") then
            participant(line:match("^%a+%s+(.+)$"))
        elseif line:match("^alt%s") or line:match("^opt%s") or line:match("^loop%s")
            or line:match("^par%s") or line == "alt" or line == "opt" or line == "loop" then
            seq.steps[#seq.steps + 1] = {
                kind = "block",
                keyword = line:match("^(%a+)"),
                text = trim(line:match("^%a+%s+(.*)$") or ""),
            }
        elseif line:match("^else%s?") then
            seq.steps[#seq.steps + 1] = {
                kind = "branch",
                keyword = "else",
                text = trim(line:match("^%a+%s+(.*)$") or ""),
            }
        elseif line == "end" then
            seq.steps[#seq.steps + 1] = { kind = "block_end" }
        elseif line:match("^[Nn]ote%s") then
            local over, text = line:match("^[Nn]ote%s+%a+%s+([^:]+):%s*(.*)$")

            if over ~= nil then
                seq.steps[#seq.steps + 1] = { kind = "note", text = trim(text) }
            end
        else
            local from, arrow, to, text =
                line:match("^([%w_]+)%s*([%-<>x%.)]+)%s*([%w_]+)%s*:%s*(.*)$")

            if from == nil then
                from, arrow, to = line:match("^([%w_]+)%s*([%-<>x%.)]+)%s*([%w_]+)%s*$")
                text = ""
            end
            if from == nil then
                -- loop, alt, else, end and the like are not drawn
                if not (line:match("^%a+$") or line:match("^%a+%s")) then
                    return nil
                end
            else
                local a = participant(seq.names[from] or from)
                local b = participant(seq.names[to] or to)

                seq.steps[#seq.steps + 1] = {
                    kind = "message",
                    from = a,
                    to = b,
                    text = trim(text),
                    dashed = arrow:find("%-%-") ~= nil,
                }
            end
        end
    end
    if #seq.order == 0 or #seq.steps == 0 then
        return nil
    end
    return seq
end

-- The lifelines of the participants, with a row per message: the text of a
-- message sits over the arrow, the head of the arrow is at the one it goes
-- to, and a branch of the diagram is framed.  The names are written again
-- under the last row, the way mermaid writes them.
local function draw_sequence(seq)
    local names = seq.order
    local widths = {}
    local gaps = {}
    local positions = {}
    local column = {}

    for i, name in ipairs(names) do
        widths[i] = width(name)
        column[name] = i
        gaps[i] = 4
    end

    local function place()
        local pos = 0

        for i = 1, #names do
            positions[i] = pos + widths[i] // 2 + 1
            pos = pos + widths[i] + gaps[i]
        end
        return math.max(pos - gaps[#names], 1)
    end

    local total = place()

    -- the room between two lifelines holds the text of every message that
    -- runs between them
    for _ = 1, 8 do
        local grew = false

        for _, step in ipairs(seq.steps) do
            if step.kind == "message" and step.text ~= "" then
                local a = column[step.from]
                local b = column[step.to]
                local lo = math.min(a, b)
                local hi = math.max(a, b)
                local room = math.abs(positions[b] - positions[a]) - 1
                local want = width(step.text) + 2

                if hi > lo and room < want then
                    local add = (want - room + (hi - lo) - 1) // (hi - lo)

                    for k = lo, hi - 1 do
                        gaps[k] = gaps[k] + add
                    end
                    grew = true
                end
            end
        end
        if not grew then
            break
        end
        total = place()
    end

    -- a frame stands to the left of the lifelines and closes to the right
    local max_depth, depth = 0, 0

    for _, step in ipairs(seq.steps) do
        if step.kind == "block" then
            depth = depth + 1
            max_depth = math.max(max_depth, depth)
        elseif step.kind == "block_end" then
            depth = math.max(depth - 1, 0)
        end
    end

    local margin = max_depth * 2
    local line_width = margin + total + margin
    local rows = {}

    local function blank()
        local row = {}

        for i = 1, line_width do
            row[i] = " "
        end
        return row
    end

    local function put(row, at, text)
        local i = at

        for _, ch in ipairs(chars(text)) do
            if i >= 1 and i <= line_width then
                row[i] = ch
            end
            i = i + 1
        end
    end

    local function lifeline_x(i)
        return margin + positions[i]
    end

    local function lifelines(row)
        for i = 1, #names do
            if row[lifeline_x(i)] == " " then
                row[lifeline_x(i)] = BAR
            end
        end
    end

    local function add(row)
        rows[#rows + 1] = row
        return #rows
    end

    local function names_row()
        local row = blank()

        for i = 1, #names do
            put(row, math.max(lifeline_x(i) - widths[i] // 2, 1), names[i])
        end
        return row
    end

    add(names_row())

    local spacer = blank()

    lifelines(spacer)
    add(spacer)

    local stack = {}
    local frames = {}

    for _, step in ipairs(seq.steps) do
        if step.kind == "block" then
            local row = blank()

            lifelines(row)
            stack[#stack + 1] = {
                depth = #stack + 1,
                top = add(row),
                label = trim(step.keyword .. " " .. step.text),
                dividers = {},
            }
        elseif step.kind == "branch" then
            local frame = stack[#stack]

            if frame ~= nil then
                local row = blank()

                lifelines(row)
                frame.dividers[#frame.dividers + 1] = {
                    row = add(row),
                    label = trim(step.keyword .. " " .. step.text),
                }
            end
        elseif step.kind == "block_end" then
            local frame = table.remove(stack)

            if frame ~= nil then
                local row = blank()

                lifelines(row)
                frame.bottom = add(row)
                frames[#frames + 1] = frame
            end
        elseif step.kind == "note" then
            local row = blank()

            lifelines(row)
            put(row, margin + 1, step.text)
            add(row)
        else
            local a_pos = lifeline_x(column[step.from])
            local b_pos = lifeline_x(column[step.to])
            local left = math.min(a_pos, b_pos)
            local right = math.max(a_pos, b_pos)
            local label = blank()
            local line = blank()

            lifelines(label)
            lifelines(line)
            if step.text ~= "" then
                local room = math.max(right - left - 1, 1)

                put(label, left + 1 + math.max((room - width(step.text)) // 2, 0), step.text)
            end
            for i = left, right do
                line[i] = (not step.dashed or i % 2 == 0) and DASH or " "
            end
            line[b_pos] = b_pos == right and ARROW_DOWN or ARROW_LEFT
            line[a_pos] = BAR
            add(label)
            add(line)
        end
        local gap_row = blank()

        lifelines(gap_row)
        add(gap_row)
    end

    add(names_row())

    -- the frames are drawn last: they run over the rows they hold
    for _, frame in ipairs(frames) do
        local x1 = margin - frame.depth * 2 + 1
        local x2 = line_width - (frame.depth - 1) * 2

        local function edge(row_index, left_ch, right_ch, label)
            local row = rows[row_index]

            for i = x1, x2 do
                row[i] = DASH
            end
            row[x1] = left_ch
            row[x2] = right_ch
            if label ~= nil and label ~= "" then
                put(row, x1 + 2, " " .. label .. " ")
            end
        end

        edge(frame.top, "\u{250C}", "\u{2510}", frame.label)
        for _, divider in ipairs(frame.dividers) do
            edge(divider.row, "\u{251C}", "\u{2524}", divider.label)
        end
        edge(frame.bottom, "\u{2514}", "\u{2518}")
        for i = frame.top + 1, frame.bottom - 1 do
            local row = rows[i]

            if row[x1] == " " then
                row[x1] = BAR
            end
            if row[x2] == " " then
                row[x2] = BAR
            end
        end
    end

    local out = {}

    for i, row in ipairs(rows) do
        out[i] = (table.concat(row):gsub("%s+$", ""))
    end
    return out
end

------------------------------------------------------------------------

-- The lines of a mermaid diagram, or nil when it is not one this draws.
function M.render(code, width_limit)
    local lines = {}

    for line in (code .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end

    local first = nil
    for _, line in ipairs(lines) do
        if trim(line) ~= "" then
            first = trim(line)
            break
        end
    end
    if first == nil then
        return nil
    end

    if first:match("^sequenceDiagram") then
        local seq = parse_sequence(lines)

        return seq ~= nil and draw_sequence(seq) or nil
    end
    if first:match("^classDiagram") then
        local model = parse_class(lines)

        return model ~= nil and draw_class(model, width_limit) or nil
    end
    if first:match("^graph%s") or first:match("^flowchart%s") then
        local chart = parse_flowchart(lines)

        if chart ~= nil then
            -- boxes when they fit on the screen, a tree when they do not
            local boxes = draw_flowchart_boxes(chart, width_limit)

            if boxes ~= nil then
                return boxes
            end
        end

        return chart ~= nil and draw_flowchart(chart) or nil
    end
    return nil
end

return M
