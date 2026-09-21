-- The document as lines: how a line is cut out of the text, what kind of
-- block it opens, and how the blocks that run over several lines are read.
-- Nothing here renders, so the caller that draws a document and the one
-- that only measures it read the same blocks.

local txt = require("text")

local trim = txt.trim
local expand_tabs = txt.expand_tabs

local M = {}


local function split_lines(text)
    local lines = {}
    local pos = 1
    local size = #text

    while pos <= size do
        local nl = text:find("\n", pos, true)
        local last = (nl or size + 1) - 1

        if last >= pos and text:byte(last) == 13 then
            last = last - 1
        end
        lines[#lines + 1] = text:sub(pos, last)
        if nl == nil then
            break
        end
        pos = nl + 1
    end
    return lines
end

-- Display math on lines of its own ($ or $$, the formula, $ or $$ again)
-- becomes one line.
local function join_display_math(lines)
    local out = {}
    local buf = nil
    local delim
    for _, line in ipairs(lines) do
        if buf ~= nil then
            local t = trim(line)

            if t == delim then
                out[#out + 1] = delim .. table.concat(buf, " ") .. delim
                buf = nil
            else
                buf[#buf + 1] = t
            end
        elseif line:find("$", 1, true) == nil then
            -- Only a line holding nothing but the delimiter opens display math.
            out[#out + 1] = line
        else
            local t = trim(line)

            if t == "$" or t == "$$" then
                buf = {}
                delim = t
            else
                out[#out + 1] = line
            end
        end
    end
    if buf ~= nil then
        out[#out + 1] = delim
        for i = 1, #buf do
            out[#out + 1] = buf[i]
        end
    end
    return out
end

local function is_blank(line)
    return line == nil or line:match("^%s*$") ~= nil
end

local function is_hr(line)
    local ch = line:match("^ ? ? ?([-*_])[%s%-*_]*$")
    return ch ~= nil and select(2, line:gsub("%" .. ch, "")) >= 3
        and line:gsub("[%s%" .. ch .. "]", "") == ""
end

-- The opening fence of a code block: its character and length.
local function fence_of(line)
    local fence = line:match("^ ? ? ?(```+)") or line:match("^ ? ? ?(~~~+)")
    return fence
end

local function list_item(line)
    local indent, marker, rest = line:match("^(%s*)([-*+])%s+(.*)$")
    if indent == nil then
        indent, marker, rest = line:match("^(%s*)(%d+[.)])%s+(.*)$")
    end
    if indent == nil then
        return nil
    end
    return indent, marker, rest
end

-- "term" on one line and ": what it means" under it
local function definition_item(line)
    return line:match("^ ? ? ?:%s+(.*)$")
end

local function is_atx_heading(line)
    return line:match("^ ? ? ?#+$") ~= nil or line:match("^ ? ? ?#+%s") ~= nil
end

local function split_row(line)
    line = line:gsub("^%s*|", ""):gsub("|%s*$", "")
    local fields = {}
    local cur = {}
    local i = 1
    while i <= #line do
        local ch = line:sub(i, i)
        if ch == "\\" and line:sub(i + 1, i + 1) == "|" then
            cur[#cur + 1] = "|"
            i = i + 2
        elseif ch == "|" then
            fields[#fields + 1] = trim(table.concat(cur))
            cur = {}
            i = i + 1
        else
            cur[#cur + 1] = ch
            i = i + 1
        end
    end
    fields[#fields + 1] = trim(table.concat(cur))
    return fields
end

local function is_table_sep(line)
    local fields = split_row(line)
    if #fields < 2 then
        return false
    end
    for _, f in ipairs(fields) do
        if not f:match("^:?%-+:?$") then
            return false
        end
    end
    return true
end


-- Whether the line opens a block of its own, which ends the paragraph
-- before it.
local function starts_block(line, next_line)
    return is_blank(line) or fence_of(line) ~= nil or is_hr(line) or is_atx_heading(line)
        or list_item(line) ~= nil or definition_item(line) ~= nil or line:match("^%s*>") ~= nil
        or (line:find("|", 1, true) ~= nil and next_line ~= nil and is_table_sep(next_line))
        or line:match("^%s*%$%$.*\\begin{") ~= nil
end

-- The fenced block that opens at lines[i]: its language, its lines of code
-- and the line after the closing fence.  A block left open runs to the end.
local function read_fence(lines, i, fence)
    local fence_char = fence:sub(1, 1)
    local language = trim(lines[i]:match("^ ? ? ?[`~]+(.*)$") or ""):match("^([%w+#._-]*)")
    local close_pattern = "^ ? ? ?(" .. (fence_char == "`" and "```+" or "~~~+") .. ")%s*$"
    local code = {}

    i = i + 1
    while i <= #lines do
        local close = lines[i]:match(close_pattern)

        if close ~= nil and #close >= #fence then
            break
        end
        code[#code + 1] = lines[i]
        i = i + 1
    end
    return language, code, i + 1
end

-- The block of code that starts at lines[i], written with four columns of
-- indent, and the line after it.  A blank line belongs to the block only
-- when the block goes on after it.  The four columns the block is written
-- with are the ones it is drawn with, so they are taken off here and put
-- back when it is drawn.
local function read_indented(lines, i)
    local block = {}

    while i <= #lines and (lines[i]:match("^    ") or lines[i]:match("^\t") or is_blank(lines[i])) do
        local next_line = lines[i + 1]

        if is_blank(lines[i])
            and not (next_line and (next_line:match("^    ") or next_line:match("^\t"))) then
            break
        end
        block[#block + 1] = expand_tabs(lines[i]):gsub("^    ", "", 1)
        i = i + 1
    end
    return block, i
end

M.split_row = split_row
M.is_table_sep = is_table_sep
M.split_lines = split_lines
M.join_display_math = join_display_math
M.is_blank = is_blank
M.is_hr = is_hr
M.fence_of = fence_of
M.list_item = list_item
M.definition_item = definition_item
M.is_atx_heading = is_atx_heading
M.starts_block = starts_block
M.read_fence = read_fence
M.read_indented = read_indented

return M
