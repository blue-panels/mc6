-- Text as the terminal measures it: characters, not bytes, and columns, not
-- characters.  A stray byte that is not UTF-8 counts as one character and is
-- kept, so that a broken file still reads.

local cfg = require("config")

local M = {}

-- a space no line is broken at; written out as a plain space
M.NBSP = "\u{00A0}"

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
        if i + len - 1 > n then
            len = n - i + 1
        end
        for k = 1, len - 1 do
            local c = s:byte(i + k)
            if c < 0x80 or c > 0xBF then
                len = k
                break
            end
        end
        out[#out + 1] = s:sub(i, i + len - 1)
        i = i + len
    end
    return out
end

-- whether ch is a combining mark, which takes no room of its own
local function is_combining(ch)
    if #ch < 2 then
        return false
    end
    local code = utf8.codepoint(ch)
    return (code >= 0x0300 and code <= 0x036F) or (code >= 0x20D0 and code <= 0x20F0)
        or (code >= 0xFE00 and code <= 0xFE0F) or code == 0x200B or code == 0x200D
end

-- Characters two columns wide: the East Asian Wide and Fullwidth blocks and
-- the emoji, as the terminal draws them.
local wide_ranges = {
    { 0x1100, 0x115F }, { 0x2E80, 0x303E }, { 0x3041, 0x33FF }, { 0x3400, 0x4DBF },
    { 0x4E00, 0x9FFF }, { 0xA000, 0xA4CF }, { 0xA960, 0xA97F }, { 0xAC00, 0xD7A3 },
    { 0xF900, 0xFAFF }, { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE6F }, { 0xFF00, 0xFF60 },
    { 0xFFE0, 0xFFE6 }, { 0x17000, 0x18AFF }, { 0x1F300, 0x1F64F }, { 0x1F680, 0x1F6FF },
    { 0x1F900, 0x1F9FF }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
}

-- The columns ch takes on screen.  ASCII is the common case and skips the
-- table; a combining mark takes none.
local function char_width(ch)
    local byte = ch:byte(1)

    if byte == nil or byte < 0x80 then
        return 1
    end
    if is_combining(ch) then
        return 0
    end

    local code = utf8.codepoint(ch)
    local lo, hi = 1, #wide_ranges

    if code < wide_ranges[1][1] then
        return 1
    end
    while lo <= hi do
        local mid = (lo + hi) // 2
        local range = wide_ranges[mid]

        if code < range[1] then
            hi = mid - 1
        elseif code > range[2] then
            lo = mid + 1
        else
            return 2
        end
    end
    return 1
end

local function width(s)
    local n = 0
    for _, ch in ipairs(chars(s)) do
        n = n + char_width(ch)
    end
    return n
end

-- Every line of the document is trimmed, so walk the ends instead of
-- rewriting the string twice.
-- The cells a string takes on a grid that gives one cell to a character,
-- which is what a diagram drawn on a canvas counts in.  This is not the
-- width on screen: a wide character takes one cell but two columns.
local function char_count(s)
    return #chars(s)
end

local function trim(s)
    local first = s:find("%S")

    if first == nil then
        return ""
    end
    local last = #s
    local c = s:byte(last)
    while c == 32 or (c >= 9 and c <= 13) do
        last = last - 1
        c = s:byte(last)
    end
    return s:sub(first, last)
end

-- Tabs in code move to the next stop; the viewer draws a tab by its own
-- rules, and the block is indented, so the stops would not line up.
local function expand_tabs(line)
    if not line:find("\t", 1, true) then
        return line
    end
    local out = {}
    local column = 0

    for _, ch in ipairs(chars(line)) do
        if ch == "\t" then
            local fill = cfg.TAB_WIDTH - column % cfg.TAB_WIDTH

            out[#out + 1] = (" "):rep(fill)
            column = column + fill
        else
            out[#out + 1] = ch
            column = column + char_width(ch)
        end
    end
    return table.concat(out)
end

M.chars = chars
M.is_combining = is_combining
M.char_width = char_width
M.width = width
M.char_count = char_count
M.trim = trim
M.expand_tabs = expand_tabs

return M
