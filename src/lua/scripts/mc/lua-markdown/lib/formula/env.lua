-- Environments in display math: matrices and the like become a block of
-- lines, the columns lined up, the brackets built from Unicode pieces.

local txt = require("text")
local latex = require("formula.latex")

local width = txt.width
local trim = txt.trim
local render_math = latex.render

local M = {}


-- Bracket pieces: one row, top, middle (the extension), bottom and, for a
-- brace, the piece that points at the middle row.
local brackets = {
    ["("] = { "(", "\u{239B}", "\u{239C}", "\u{239D}" },
    [")"] = { ")", "\u{239E}", "\u{239F}", "\u{23A0}" },
    ["["] = { "[", "\u{23A1}", "\u{23A2}", "\u{23A3}" },
    ["]"] = { "]", "\u{23A4}", "\u{23A5}", "\u{23A6}" },
    -- a brace is drawn with rounded box corners: the pieces of the real brace
    -- do not line up in most terminal fonts
    ["{"] = { "{", "\u{256D}", "\u{2502}", "\u{2570}", "\u{2524}" },
    ["}"] = { "}", "\u{256E}", "\u{2502}", "\u{256F}", "\u{251C}" },
    ["|"] = { "\u{2502}", "\u{2502}", "\u{2502}", "\u{2502}" },
    ["||"] = { "\u{2016}", "\u{2016}", "\u{2016}", "\u{2016}" },
}

-- environment: left bracket, right bracket, column alignment
local environments = {
    matrix = { nil, nil, "c" },
    smallmatrix = { nil, nil, "c" },
    pmatrix = { "(", ")", "c" },
    bmatrix = { "[", "]", "c" },
    Bmatrix = { "{", "}", "c" },
    vmatrix = { "|", "|", "c" },
    Vmatrix = { "||", "||", "c" },
    cases = { "{", nil, "l" },
    array = { nil, nil, "c" },
    aligned = { nil, nil, "rl" },
    align = { nil, nil, "rl" },
    ["align*"] = { nil, nil, "rl" },
    gathered = { nil, nil, "c" },
    split = { nil, nil, "rl" },
}

-- The piece of a bracket for row k of n.
local function bracket_piece(name, k, n)
    local b = brackets[name]
    if n == 1 then
        return b[1]
    elseif k == 1 then
        return b[2]
    elseif k == n then
        return b[4]
    elseif b[5] ~= nil and k == (n + 1) // 2 then
        return b[5]
    end
    return b[3]
end

local render_math_inline

-- The rows and cells of the body of an environment.  A nested environment
-- keeps its own separators: only those outside every \begin ... \end count.
local function split_env_body(body)
    local rows = {}
    local cells = {}
    local piece = {}
    local depth = 0
    local i = 1

    local function end_cell()
        cells[#cells + 1] = trim(table.concat(piece))
        piece = {}
    end

    local function end_row()
        end_cell()
        if #cells > 1 or cells[1] ~= "" then
            rows[#rows + 1] = cells
        end
        cells = {}
    end

    while i <= #body do
        local two = body:sub(i, i + 1)

        if body:find("^\\begin{", i) then
            depth = depth + 1
            piece[#piece + 1] = body:sub(i, i + 6)
            i = i + 7
        elseif body:find("^\\end{", i) then
            depth = depth - 1
            piece[#piece + 1] = body:sub(i, i + 4)
            i = i + 5
        elseif depth == 0 and two == "\\\\" then
            end_row()
            i = i + 2
        elseif depth == 0 and body:sub(i, i) == "&" then
            end_cell()
            i = i + 1
        else
            piece[#piece + 1] = body:sub(i, i)
            i = i + 1
        end
    end
    end_row()
    return rows
end

-- What an environment holds: the text before and after it, its rows of
-- rendered cells and how its columns line up.  nil when this is not an
-- environment it knows.
local function parse_math_env(formula)
    local before, name, rest = formula:match("^(.-)\\begin{([%a*]+)}(.*)$")
    local env = name and environments[name]
    if env == nil then
        return nil
    end
    -- an environment of the same name inside this one has its own end
    local open_tag = "\\begin{" .. name .. "}"
    local close_tag = "\\end{" .. name .. "}"
    local depth = 1
    local at = 1
    local close = nil

    while true do
        local next_open = rest:find(open_tag, at, true)
        local next_close = rest:find(close_tag, at, true)

        if next_close == nil then
            break
        end
        if next_open ~= nil and next_open < next_close then
            depth = depth + 1
            at = next_open + #open_tag
        else
            depth = depth - 1
            if depth == 0 then
                close = next_close
                break
            end
            at = next_close + #close_tag
        end
    end
    if close == nil then
        return nil
    end
    local body = rest:sub(1, close - 1)
    local after = rest:sub(close + #close_tag)
    local align = env[3]

    if name == "array" then
        local spec, tail = body:match("^%s*{([^}]*)}(.*)$")
        if spec ~= nil then
            align = spec:gsub("[^lcr]", "")
            body = tail
        end
    end

    local rows = {}
    local ncols = 0

    for _, raw in ipairs(split_env_body(body)) do
        local cells = {}

        for _, cell in ipairs(raw) do
            -- an environment inside a cell is drawn on one line
            local inner = cell:find("\\begin{", 1, true) and render_math_inline(cell) or nil

            cells[#cells + 1] = inner or render_math(cell)
        end
        rows[#rows + 1] = cells
        ncols = math.max(ncols, #cells)
    end
    if #rows == 0 then
        return nil
    end
    return {
        env = env,
        rows = rows,
        ncols = ncols,
        align = align,
        lead = render_math(trim(before)),
        tail = render_math(trim(after)),
    }
end

-- An environment inside a formula in the text: one line, rows told apart by
-- a semicolon, because the line it sits on has one row of its own.
function render_math_inline(formula)
    local m = parse_math_env(formula)
    if m == nil then
        return nil
    end
    local rows = {}

    for _, cells in ipairs(m.rows) do
        rows[#rows + 1] = table.concat(cells, " ")
    end
    local body = table.concat(rows, "; ")
    -- on one line a bracket is closed even where the block leaves it open,
    -- as cases does
    local mirror = { ["("] = ")", ["["] = "]", ["{"] = "}", ["|"] = "|", ["||"] = "||" }
    local open = m.env[1] ~= nil and brackets[m.env[1]][1] or ""
    local close = m.env[2] ~= nil and brackets[m.env[2]][1] or ""

    if close == "" and m.env[1] ~= nil then
        close = brackets[mirror[m.env[1]]][1]
    end

    return trim(m.lead .. " " .. open .. body .. close .. " " .. m.tail)
end

-- The lines of display math that holds an environment, or nil if it holds
-- none this knows.  Text before and after the environment goes on its
-- middle row.
local function render_math_block(formula, width_limit)
    local m = parse_math_env(formula)
    if m == nil then
        return nil
    end
    local colw = {}
    for c = 1, m.ncols do
        colw[c] = 0
        for _, cells in ipairs(m.rows) do
            colw[c] = math.max(colw[c], width(cells[c] or ""))
        end
    end

    -- the columns of a wide block are set closer together before anything
    -- else is given up
    local gap = m.align == "rl" and " " or "  "
    if width_limit ~= nil then
        local total = 4 + (m.ncols - 1) * #gap + width(m.lead) + width(m.tail) + 4
        for c = 1, m.ncols do
            total = total + colw[c]
        end
        if total > width_limit and m.align ~= "rl" then
            gap = " "
        end
    end

    local n = #m.rows
    local middle = (n + 1) // 2
    local lines = {}

    for k, cells in ipairs(m.rows) do
        local parts = {}

        for c = 1, m.ncols do
            local cell = cells[c] or ""
            local pad = colw[c] - width(cell)
            local a = m.align:sub(c, c)

            if a == "" then
                a = m.align:sub(-1)
            end
            if m.align == "rl" then
                a = c % 2 == 1 and "r" or "l"
            end
            if a == "r" then
                parts[c] = (" "):rep(pad) .. cell
            elseif a == "c" then
                parts[c] = (" "):rep(pad // 2) .. cell .. (" "):rep(pad - pad // 2)
            else
                parts[c] = cell .. (" "):rep(pad)
            end
        end

        local line = table.concat(parts, gap)

        if m.env[1] ~= nil then
            line = bracket_piece(m.env[1], k, n) .. " " .. line
        end
        if m.env[2] ~= nil then
            line = line .. " " .. bracket_piece(m.env[2], k, n)
        end
        if m.lead ~= "" then
            line = (k == middle and m.lead .. " " or (" "):rep(width(m.lead) + 1)) .. line
        end
        if m.tail ~= "" and k == middle then
            line = line .. " " .. m.tail
        end
        lines[#lines + 1] = ("    " .. line):gsub("%s+$", "")
    end
    return lines
end

-- The block lines of a line that is display math with an environment.
local function math_block_of(line, width_limit)
    local formula = trim(line):match("^%$%$(.*)%$%$$")
    return formula ~= nil and formula:find("\\begin{", 1) and render_math_block(formula, width_limit)
        or nil
end

-- The closing $ of the formula that opens at pos (the position after the
-- opening delimiter), or nil.  The text is math only when the delimiters
-- hug it and the next $ closes it, so "$5 and $10" stays what it is.
local function find_math_end(s, pos, delim)
    if s:sub(pos, pos):match("^%s$") or s:sub(pos, pos) == "" then
        return nil
    end
    local a, b = s:find(delim, pos, true)
    if a == nil then
        return nil
    end
    local before = s:sub(a - 1, a - 1)
    local after = s:sub(b + 1, b + 1)
    if a > pos and not before:match("^%s$") and not after:match("^%d$") then
        return a, b
    end
    return nil
end


M.render_inline = render_math_inline
M.block_of = math_block_of
M.find_end = find_math_end

return M
