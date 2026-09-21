-- A pie chart, drawn as a row of bars.

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

-- The eighths a bar is drawn with: a full cell and the seven parts of one.
local PIE_FULL = "\u{2588}"
local PIE_PARTS = {
    "\u{258F}", "\u{258E}", "\u{258D}", "\u{258C}",
    "\u{258B}", "\u{258A}", "\u{2589}", PIE_FULL,
}

-- the widest a chart gets, however much room the screen leaves
local PIE_WIDTH = 60

-- "pie showData title Parts", then a "label" : value per line.
local function parse_pie(lines)
    local pie = { slices = {} }

    for _, raw in ipairs(lines) do
        local line = trim(raw:gsub("%%%%.*$", ""))
        local head = line:match("^pie%s*(.*)$")

        if head ~= nil then
            if head:match("^showData") then
                pie.show_data = true
                head = trim(head:match("^showData%s*(.*)$"))
            end
            local title = head:match("^title%s+(.+)$")

            if title ~= nil then
                pie.title = trim(title)
            end
        elseif line ~= "" then
            local title = line:match("^title%s+(.+)$")
            local label, value = line:match('^"(.-)"%s*:%s*([%d.]+)$')

            if label == nil then
                label, value = line:match("^(.-)%s*:%s*([%d.]+)$")
            end
            if title ~= nil then
                pie.title = trim(title)
            elseif label ~= nil and tonumber(value) ~= nil then
                pie.slices[#pie.slices + 1] = { label = trim(label), value = tonumber(value) }
            end
        end
    end
    if #pie.slices == 0 then
        return nil
    end
    return pie
end

-- A bar per slice, the smallest first, each with the share it takes.  A
-- circle cut into sectors says less on a terminal than a row of bars does.
local function draw_pie(pie, width_limit)
    local total = 0
    local label_width = 0
    local share_width = 0
    local value_width = 0
    local order = {}

    for i, slice in ipairs(pie.slices) do
        total = total + slice.value
        order[i] = i
    end
    if total <= 0 then
        return nil
    end

    table.sort(order, function(a, b)
        local one = pie.slices[a]
        local two = pie.slices[b]

        if one.value ~= two.value then
            return one.value < two.value
        end
        return a < b
    end)

    local most = pie.slices[order[#order]].value

    for _, slice in ipairs(pie.slices) do
        slice.share = ("%.1f%%"):format(slice.value / total * 100)
        slice.text = ("%g"):format(slice.value)
        label_width = math.max(label_width, width(slice.label))
        share_width = math.max(share_width, width(slice.share))
        value_width = math.max(value_width, width(slice.text))
    end

    local room = math.min(width_limit or PIE_WIDTH, PIE_WIDTH)
    local taken = label_width + 2 + share_width + 2 + (pie.show_data and value_width + 2 or 0)
    local bar = math.max(room - taken, 8)
    local out = {}

    if pie.title ~= nil and pie.title ~= "" then
        out[#out + 1] = pie.title
        out[#out + 1] = ""
    end
    for _, i in ipairs(order) do
        local slice = pie.slices[i]
        local eighths = math.floor(slice.value / most * bar * 8 + 0.5)
        local cells = eighths // 8
        local rest = eighths % 8
        local drawn = PIE_FULL:rep(cells) .. (rest > 0 and PIE_PARTS[rest] or "")

        if drawn == "" then
            drawn = PIE_PARTS[1]
        end
        local line = slice.label
            .. (" "):rep(label_width - width(slice.label) + 2)
            .. drawn
            .. (" "):rep(bar - width(drawn) + 2)
            .. (" "):rep(share_width - width(slice.share))
            .. slice.share

        if pie.show_data then
            line = line .. (" "):rep(value_width - width(slice.text) + 2) .. slice.text
        end
        out[#out + 1] = line
    end
    return out
end

M.parse = parse_pie
M.draw = draw_pie

return M
