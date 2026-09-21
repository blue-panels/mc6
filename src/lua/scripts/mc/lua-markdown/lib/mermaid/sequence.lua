-- A sequence diagram: the lifelines of the participants, with a row per
-- message.

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

local canvas_new = cv.new
local canvas_pen = cv.pen
local canvas_paint = cv.paint
local canvas_put = cv.put
local canvas_line = cv.line
local canvas_draw_lines = cv.draw_lines
local canvas_lines = cv.lines
local decision_edges = cv.decision_edges

local M = {}

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


M.parse = parse_sequence
M.draw = draw_sequence

return M
