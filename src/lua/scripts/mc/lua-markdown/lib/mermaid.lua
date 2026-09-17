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
        chart.nodes[id] = { id = id, text = "[" .. id .. "]" }
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
    node.text = node_text(trim(label:gsub('^"(.*)"$', "%1")), shape)
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
        local id, label = line:match("^participants?%s+([%w_]+)%s+as%s+(.+)$")

        if line == "" or line == "sequenceDiagram" then
            -- the header
        elseif id ~= nil then
            participant(trim(label))
            seq.names[id] = trim(label)
        elseif line:match("^participants?%s+") then
            participant(line:match("^participants?%s+(.+)$"))
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
-- message sits over the arrow, and the head of the arrow is at the one it
-- goes to.
local function draw_sequence(seq)
    local out = {}
    local names = seq.order
    local gap = 4
    local widths = {}
    local positions = {}
    local column = {}
    local pos = 0

    for i, name in ipairs(names) do
        widths[i] = width(name)
        positions[i] = pos + widths[i] // 2 + 1
        column[name] = i
        pos = pos + widths[i] + gap
    end
    local total = math.max(pos - gap, 1)

    local function blank()
        local row = {}

        for i = 1, total do
            row[i] = " "
        end
        return row
    end

    local function put(row, at, text)
        local i = at

        for _, ch in ipairs(chars(text)) do
            if i >= 1 and i <= total then
                row[i] = ch
            end
            i = i + 1
        end
    end

    local function lifelines(row)
        for i = 1, #names do
            if row[positions[i]] == " " then
                row[positions[i]] = BAR
            end
        end
    end

    local head = blank()
    local at = 1
    for i = 1, #names do
        put(head, at, names[i])
        at = at + widths[i] + gap
    end
    out[#out + 1] = table.concat(head)

    local spacer = blank()
    lifelines(spacer)
    out[#out + 1] = table.concat(spacer)

    for _, step in ipairs(seq.steps) do
        if step.kind == "note" then
            local note = blank()

            lifelines(note)
            put(note, 1, step.text)
            out[#out + 1] = table.concat(note)
        else
            local a_pos = positions[column[step.from]]
            local b_pos = positions[column[step.to]]
            local left = math.min(a_pos, b_pos)
            local right = math.max(a_pos, b_pos)
            local label = blank()
            local line = blank()

            lifelines(label)
            if step.text ~= "" then
                local room = math.max(right - left - 1, 1)
                local text = step.text

                if width(text) > room then
                    text = table.concat(chars(text), "", 1, room)
                end
                put(label, left + 1 + math.max((room - width(text)) // 2, 0), text)
            end

            for i = left, right do
                line[i] = (not step.dashed or i % 2 == 0) and DASH or " "
            end
            line[b_pos] = b_pos == right and ARROW_DOWN or ARROW_LEFT
            line[a_pos] = BAR

            out[#out + 1] = table.concat(label)
            out[#out + 1] = table.concat(line)
        end
        local spacer2 = blank()

        lifelines(spacer2)
        out[#out + 1] = table.concat(spacer2)
    end

    for i, line in ipairs(out) do
        out[i] = (line:gsub("%s+$", ""))
    end
    return out
end

------------------------------------------------------------------------

-- The lines of a mermaid diagram, or nil when it is not one this draws.
function M.render(code)
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
    if first:match("^graph%s") or first:match("^flowchart%s") then
        local chart = parse_flowchart(lines)

        return chart ~= nil and draw_flowchart(chart) or nil
    end
    return nil
end

return M
