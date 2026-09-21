-- The canvas the diagrams are drawn on: a grid of cells a drawing puts
-- characters and lines into.  A line is kept as the directions it leaves a
-- cell in, so that two lines crossing the same cell are joined with the
-- character that shows both.

local txt = require("text")
local cfg = require("config")
local glyphs = require("mermaid.glyphs")

local chars = txt.chars
local width = txt.char_count  -- a canvas counts cells, not columns

local M = {}


local BRAILLE_BASE = 0x2800
local BRAILLE_BITS = {
    [0] = { 0x01, 0x02, 0x04, 0x40 },  -- left column of dots, top one first
    [1] = { 0x08, 0x10, 0x20, 0x80 },  -- right column
}

-- One row of cells built from the dots set in it.
local function braille_row(dots, width)
    local out = {}

    for cx = 0, width - 1 do
        local bits = 0

        for dx = 0, 1 do
            for dy = 0, 3 do
                if dots[dy][cx * 2 + dx] then
                    bits = bits | BRAILLE_BITS[dx][dy + 1]
                end
            end
        end
        out[#out + 1] = bits == 0 and " " or utf8.char(BRAILLE_BASE + bits)
    end
    return table.concat(out)
end

-- The top and the bottom edge of a decision: a slope up from the left point,
-- a straight run between the slopes, and the same mirrored underneath.
local function decision_edges(width)
    local top = { [0] = {}, [1] = {}, [2] = {}, [3] = {} }
    local bottom = { [0] = {}, [1] = {}, [2] = {}, [3] = {} }
    local last = width * 2 - 1
    local slope = math.min(3, last // 2)

    local function put(dots, x, y)
        if x >= 0 and x <= last and y >= 0 and y <= 3 then
            dots[y][x] = true
        end
    end

    for i = 0, slope do
        local y = 3 - (i * 3) // math.max(slope, 1)

        put(top, i, y)
        put(top, last - i, y)
        put(bottom, i, 3 - y)
        put(bottom, last - i, 3 - y)
    end
    for x = slope, last - slope do
        put(top, x, 0)
        put(bottom, x, 3)
    end
    return braille_row(top, width), braille_row(bottom, width)
end
local BOX_H, BOX_V = glyphs.DASH, glyphs.BAR

local function canvas_new()
    return { rows = {}, mask = {}, color = {}, width = 0, pen = nil }
end

-- What the lines drawn from now on are colored with; nil paints nothing.
local function canvas_pen(canvas, color)
    canvas.pen = color
end

local function canvas_paint(canvas, y, x)
    if canvas.pen ~= nil then
        canvas.color[y] = canvas.color[y] or {}
        canvas.color[y][x] = canvas.pen
    end
end

local function canvas_put(canvas, y, x, text)
    local row = canvas.rows[y] or {}
    local i = x

    for _, ch in ipairs(chars(text)) do
        row[i] = ch
        if canvas.pen ~= nil then
            canvas.color[y] = canvas.color[y] or {}
            canvas.color[y][i] = canvas.pen
        end
        i = i + 1
    end
    canvas.rows[y] = row
    canvas.width = math.max(canvas.width, i - 1)
end

-- Lines are kept as the directions they leave a cell in, and the character
-- is chosen once all of them are known.
-- the four directions a line leaves a cell in, as bits
local UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8

local LINE_GLYPH = {
    [LEFT + RIGHT] = "\u{2500}", [LEFT] = "\u{2500}", [RIGHT] = "\u{2500}",
    [UP + DOWN] = "\u{2502}", [UP] = "\u{2502}", [DOWN] = "\u{2502}",
    -- a line turns with a rounded corner; the boxes keep the square ones
    [DOWN + RIGHT] = "\u{256D}", [DOWN + LEFT] = "\u{256E}",
    [UP + RIGHT] = "\u{2570}", [UP + LEFT] = "\u{256F}",
    [UP + DOWN + RIGHT] = "\u{251C}", [UP + DOWN + LEFT] = "\u{2524}",
    [DOWN + LEFT + RIGHT] = "\u{252C}", [UP + LEFT + RIGHT] = "\u{2534}",
    [UP + DOWN + LEFT + RIGHT] = "\u{253C}",
}

-- One cell of a line.  Cells of the same line are joined, so that a turn
-- reads as a turn; where two lines cross, the one going down is drawn over
-- the one going across, which is left broken there.
local function canvas_line(canvas, y, x, dirs, owner)
    local row = canvas.mask[y] or {}
    local cell = row[x]

    canvas.mask[y] = row
    canvas.width = math.max(canvas.width, x)

    if cell == nil then
        row[x] = { dirs = dirs, owner = owner }
        canvas_paint(canvas, y, x)
        return
    end
    if cell.owner == owner then
        cell.dirs = cell.dirs | dirs
        canvas_paint(canvas, y, x)
        return
    end

    local down = UP | DOWN
    local across = LEFT | RIGHT
    local crossing = (dirs == down and cell.dirs == across)
        or (dirs == across and cell.dirs == down)

    if not crossing then
        -- the lines meet rather than cross: one of them branches here
        cell.dirs = cell.dirs | dirs
        canvas_paint(canvas, y, x)
    elseif dirs == down then
        -- the line going down is drawn over the one going across
        row[x] = { dirs = dirs, owner = owner }
        canvas_paint(canvas, y, x)
    end
    -- a line going across keeps the break where another line crosses it
end

local function canvas_draw_lines(canvas)
    local pen = canvas.pen

    canvas_pen(canvas, nil)
    for y, row in pairs(canvas.mask) do
        for x, cell in pairs(row) do
            local was = canvas.rows[y] ~= nil and canvas.rows[y][x] or nil

            if was == nil or was == " " then
                canvas_put(canvas, y, x, LINE_GLYPH[cell.dirs] or "\u{253C}")
            end
        end
    end
    canvas_pen(canvas, pen)
end

local function canvas_lines(canvas)
    local out = {}
    local last = 0

    for y in pairs(canvas.rows) do
        last = math.max(last, y)
    end
    for y = 1, last do
        local row = canvas.rows[y] or {}
        local colors = canvas.color[y] or {}
        local line = {}
        local pen = nil

        for x = 1, canvas.width do
            local color = colors[x]

            -- a color is opened where it starts and closed where it ends, so
            -- that a line can be read from any row
            if color ~= pen then
                if pen ~= nil then
                    line[#line + 1] = "\27[39m"
                end
                if color ~= nil then
                    line[#line + 1] = "\27[" .. color .. "m"
                end
                pen = color
            end
            line[#line + 1] = row[x] or " "
        end
        if pen ~= nil then
            line[#line + 1] = "\27[39m"
        end
        out[y] = (table.concat(line):gsub("%s+(\27%[39m)$", "%1"):gsub("%s+$", ""))
    end
    return out
end

M.UP, M.DOWN, M.LEFT, M.RIGHT = UP, DOWN, LEFT, RIGHT
M.decision_edges = decision_edges
M.new = canvas_new
M.pen = canvas_pen
M.paint = canvas_paint
M.put = canvas_put
M.line = canvas_line
M.draw_lines = canvas_draw_lines
M.lines = canvas_lines

return M
