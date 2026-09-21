-- A rendered line, cut into units: one visible character with the
-- overstrikes that style it and the escape sequences that surround it.  A
-- unit is what the wrapping moves around, because an escape sequence takes
-- no columns and an overstruck character is more than one byte.

local txt = require("text")
local sgr = require("sgr")

local chars = txt.chars
local char_width = txt.char_width
local is_combining = txt.is_combining

local SGR_ITALIC, SGR_ITALIC_OFF = sgr.SGR_ITALIC, sgr.SGR_ITALIC_OFF
local SGR_COLOR_OFF, LINK_END = sgr.SGR_COLOR_OFF, sgr.LINK_END
local link_start = sgr.link_start

local M = {}

-- marks a unit that takes two columns
local WIDE = "\1"

-- The visible characters of rendered text, each with the overstrikes that
-- style it (c, c\bc, _\bc, _\bc\bc).  A unit two columns wide is marked with
-- WIDE in front; the mark is dropped when the line is written out, and
-- nothing else in the text can hold that byte.
-- An SGR sequence takes no room: one that ends a style goes with the
-- character before it, any other with the character after it.
local function units_of(rendered)
    local cs = chars(rendered)
    local units = {}
    local prefix = ""
    local i = 1
    while i <= #cs do
        if cs[i] == "\27" then
            local j = i + 1
            if cs[j] == "]" then
                -- OSC, up to ST
                while cs[j] ~= nil and not (cs[j] == "\\" and cs[j - 1] == "\27") do
                    j = j + 1
                end
            else
                while cs[j] ~= nil and not (j > i + 1 and cs[j]:match("^[@-~]$")) do
                    j = j + 1
                end
            end
            local seq = table.concat(cs, "", i, math.min(j, #cs))
            if #units > 0 and (seq == SGR_ITALIC_OFF or seq == SGR_COLOR_OFF or seq == LINK_END) then
                units[#units] = units[#units] .. seq
            else
                prefix = prefix .. seq
            end
            i = j + 1
        else
            local unit = prefix .. cs[i]

            if char_width(cs[i]) == 2 then
                unit = WIDE .. unit
            end
            prefix = ""
            i = i + 1
            while cs[i] == "\b" and cs[i + 1] ~= nil do
                unit = unit .. "\b" .. cs[i + 1]
                i = i + 2
            end
            while cs[i] ~= nil and is_combining(cs[i]) do
                unit = unit .. cs[i]
                i = i + 1
            end
            units[#units + 1] = unit
        end
    end
    if prefix ~= "" and #units > 0 then
        units[#units] = units[#units] .. prefix
    end
    return units
end

-- The columns one unit takes, and the columns a run of them takes.
local function unit_width(u)
    return u:byte(1) == 1 and 2 or 1
end

local function units_width(units)
    local n = 0
    for _, u in ipairs(units) do
        n = n + (u:byte(1) == 1 and 2 or 1)
    end
    return n
end

-- One line out of wrapped units, with the SGR styles and the link open at its start
-- opened again and those still open at its end closed, so that each line
-- stands on its own: the viewer may start reading at any of them.  state
-- carries what is open from one line to the next.
local function sgr_line(units, state)
    local before = (state.italic and SGR_ITALIC or "")
        .. (state.color and "\27[" .. state.color .. "m" or "")
        .. (state.link and link_start(state.link) or "")

    for _, u in ipairs(units) do
        -- Most units contain only text; avoid allocating pattern iterators for them.
        if u:find("\27", 1, true) ~= nil then
            for url in u:gmatch("\27%]8;;(.-)\27\\") do
                state.link = url ~= "" and url or nil
            end
            for code in u:gmatch("\27%[(%d*)m") do
                if code == "3" then
                    state.italic = true
                elseif code == "23" then
                    state.italic = false
                elseif code == "39" then
                    state.color = nil
                else
                    state.color = code
                end
            end
        end
    end
    local after = (state.link and LINK_END or "")
        .. (state.color and SGR_COLOR_OFF or "")
        .. (state.italic and SGR_ITALIC_OFF or "")
    return (before .. table.concat(units) .. after):gsub(WIDE, "")
end

-- The words of rendered text, packed into lines no wider than w columns; a
-- space is never overstruck, so it is always a unit of its own.
local function wrap_units(units, w)
    local segs = {}
    local cur = {}
    local word = {}
    local cur_w, word_w = 0, 0

    local function push_word()
        if #word == 0 then
            return
        end
        while word_w > w do
            -- a word wider than the line is broken after the last separator
            -- that fits, as an address breaks after a slash
            local head, head_w = {}, 0
            local cut = 0

            if #cur > 0 then
                segs[#segs + 1] = cur
                cur, cur_w = {}, 0
            end
            while #word > 0 and head_w + unit_width(word[1]) <= w do
                head_w = head_w + unit_width(word[1])
                head[#head + 1] = table.remove(word, 1)
                if head[#head]:match("[/\\%-?&=.,;:_]$") and #head < w then
                    cut = #head
                end
            end
            -- nothing worth breaking at in the first half: cut where it ends
            if cut > w // 2 then
                for k = #head, cut + 1, -1 do
                    table.insert(word, 1, head[k])
                    head_w = head_w - unit_width(head[k])
                    head[k] = nil
                end
            end
            segs[#segs + 1] = head
            word_w = word_w - head_w
        end
        if #cur == 0 then
            cur, cur_w = word, word_w
        elseif cur_w + 1 + word_w <= w then
            cur[#cur + 1] = " "
            for _, u in ipairs(word) do
                cur[#cur + 1] = u
            end
            cur_w = cur_w + 1 + word_w
        else
            segs[#segs + 1] = cur
            cur, cur_w = word, word_w
        end
        word, word_w = {}, 0
    end

    for _, u in ipairs(units) do
        if u == " " then
            push_word()
        else
            word[#word + 1] = u
            word_w = word_w + unit_width(u)
        end
    end
    push_word()
    if #cur > 0 then
        segs[#segs + 1] = cur
    end
    if #segs == 0 then
        segs[1] = {}
    end
    return segs
end

M.WIDE = WIDE
M.of_text = units_of
M.unit_width = unit_width
M.width = units_width
M.line = sgr_line
M.wrap = wrap_units

return M
