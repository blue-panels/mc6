-- A class diagram: a box of three parts per class, the children under
-- the class they come from.

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


local TRIANGLE = "\u{25B3}"  -- the head of an inheritance arrow

-- Every class has a color of its own, cycled after the last one: its box is
-- drawn in it and so is the line that ties it to the class it comes from,
-- which is how the line is followed back to its box.  An empty list draws
-- everything in the color of the text.
local EDGE_COLORS = { "36", "33", "32", "35", "34", "31", "96", "93" }

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

-- The box of one class, drawn at a place on the canvas.
local function draw_class_box(canvas, cls, name, top, left, w)
    local at = top
    local pen = canvas.pen

    local function put_row(text, centered)
        local pad = math.max(w - 2 - width(text), 0)
        local before = centered and pad // 2 or 1

        canvas_put(canvas, at, left, BOX_V)
        canvas_pen(canvas, nil)
        canvas_put(canvas, at, left + 1,
                   (" "):rep(before) .. text .. (" "):rep(math.max(pad - before, 0)))
        canvas_pen(canvas, pen)
        canvas_put(canvas, at, left + w - 1, BOX_V)
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

-- When a row of boxes does not fit the screen, the classes are stacked one
-- under another and the lines run down the left margin.
local function draw_class_column(model, parents, children, width_limit)
    local canvas = canvas_new()
    local indent = 5
    local y = 1
    local drawn = {}
    local marks = {}

    local function box_size(name)
        local cls = model.classes[name]
        local w = width(name)

        for _, list in ipairs({ cls.attrs, cls.methods }) do
            for _, text in ipairs(list) do
                w = math.max(w, width(text))
            end
        end
        return w + 4, 3 + #cls.attrs + #cls.methods
            + (#cls.attrs > 0 and 1 or 0) + (#cls.methods > 0 and 1 or 0)
    end

    local color_of = {}

    for n, name in ipairs(model.order) do
        color_of[name] = #EDGE_COLORS > 0 and EDGE_COLORS[(n - 1) % #EDGE_COLORS + 1] or nil
    end

    local function draw(name, left)
        local w, h = box_size(name)

        if width_limit ~= nil and left + w - 1 > width_limit then
            return false
        end
        canvas_pen(canvas, color_of[name])
        draw_class_box(canvas, model.classes[name], name, y, left, w)
        canvas_pen(canvas, nil)
        drawn[name] = { top = y, height = h, left = left }
        y = y + h + 1
        return true
    end

    for _, name in ipairs(model.order) do
        if parents[name] == nil and not drawn[name] then
            if not draw(name, 1) then
                return nil
            end
            local kids = children[name] or {}

            for n, child in ipairs(kids) do
                local top = y

                if not draw(child, indent + 1) then
                    return nil
                end
                marks[#marks + 1] = { parent = name, child = child, top = top, last = n == #kids }
            end
        end
    end
    for _, name in ipairs(model.order) do
        if not drawn[name] then
            if not draw(name, 1) then
                return nil
            end
        end
    end

    -- one line down the margin, with a branch to every class that comes from
    -- the one above
    for _, mark in ipairs(marks) do
        local parent = drawn[mark.parent]
        local x = parent.left + 2
        local from = parent.top + parent.height
        local to = mark.top + 1

        -- the trunk belongs to the class every branch comes from, the branch
        -- itself to the class it runs to
        canvas_pen(canvas, color_of[mark.parent])
        canvas_put(canvas, from, x, TRIANGLE)
        canvas_paint(canvas, from, x)
        for i = from + 1, to - 1 do
            canvas_line(canvas, i, x, UP | DOWN, mark.parent)
        end
        canvas_pen(canvas, color_of[mark.child])
        canvas_line(canvas, to, x, UP | RIGHT | (mark.last and 0 or DOWN), mark.parent)
        for i = x + 1, indent do
            canvas_line(canvas, to, i, LEFT | RIGHT, mark.child)
        end
    end

    canvas_pen(canvas, nil)
    canvas_draw_lines(canvas)
    return canvas_lines(canvas)
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
            return draw_class_column(model, parents, children, width_limit)
        end

        -- every line gets a row of its own to turn in, so that the lines do
        -- not run into one another
        local turns = 1

        for _, name in ipairs(rows[level] or {}) do
            turns = math.max(turns, #(children[name] or {}))
        end
        y = y + tallest + turns + 2
    end

    local color_of = {}

    for n, name in ipairs(model.order) do
        color_of[name] = #EDGE_COLORS > 0 and EDGE_COLORS[(n - 1) % #EDGE_COLORS + 1] or nil
    end

    for _, name in ipairs(model.order) do
        canvas_pen(canvas, color_of[name])
        draw_class_box(canvas, model.classes[name], name, y_of[name], x_of[name], box_w[name])
    end
    canvas_pen(canvas, nil)

    for _, parent in ipairs(model.order) do
      local kids = children[parent]

      if kids ~= nil then
        local py = y_of[parent] + box_h[parent] - 1
        local left = x_of[parent]
        local w = box_w[parent]

        -- the line that travels farthest turns first, so that a line that
        -- goes down later never crosses one that already turned
        local order = {}

        for n = 1, #kids do
            order[n] = n
        end
        table.sort(order, function(a, b)
            local ax = x_of[kids[a]] + box_w[kids[a]] // 2
            local bx = x_of[kids[b]] + box_w[kids[b]] // 2

            return math.abs(ax - left - w // 2) > math.abs(bx - left - w // 2)
        end)
        local row_of = {}

        for place, n in ipairs(order) do
            row_of[n] = place
        end

        for n, child in ipairs(kids) do
            -- every line leaves the parent at a place of its own and carries
            -- its own arrow, the way UML draws generalization
            local ex = left + math.max(w * n // (#kids + 1), 1)
            local cx = x_of[child] + box_w[child] // 2
            local owner = parent .. ">" .. child

            -- the line belongs to the class it runs to
            canvas_pen(canvas, color_of[child])
            local cy = y_of[child]
            local bus = py + 1 + row_of[n]
            local lo = math.min(cx, ex)
            local hi = math.max(cx, ex)

            canvas_put(canvas, py + 1, ex, TRIANGLE)
            canvas_paint(canvas, py + 1, ex)
            if cx == ex then
                for i = py + 2, cy - 1 do
                    canvas_line(canvas, i, cx, UP | DOWN, owner)
                end
            else
                for i = py + 2, bus - 1 do
                    canvas_line(canvas, i, ex, UP | DOWN, owner)
                end
                canvas_line(canvas, bus, ex, UP | (cx < ex and LEFT or RIGHT), owner)
                for i = lo + 1, hi - 1 do
                    canvas_line(canvas, bus, i, LEFT | RIGHT, owner)
                end
                canvas_line(canvas, bus, cx, DOWN | (cx < ex and RIGHT or LEFT), owner)
                for i = bus + 1, cy - 1 do
                    canvas_line(canvas, i, cx, UP | DOWN, owner)
                end
            end
        end
      end
    end

    canvas_pen(canvas, nil)
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

M.parse = parse_class
M.draw = draw_class

return M
