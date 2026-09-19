-- Markdown to the nroff-style text the viewer paints: overstruck letters
-- for headings and bold, underscore overstrikes for code and links, SGR for
-- italic and for the colors of the heading levels.
-- One pass over the lines for the blocks, one tokenizing pass per line for
-- the inline markup; nothing is scanned twice.

local mermaid = require("mermaid")

local M = {}

M.MIN_COLUMN = 8     -- a table column is never squeezed narrower than this
M.DEFAULT_WIDTH = 80 -- the screen width when the caller names none
M.MAX_WIDTH = 120    -- text is never flowed wider than this, whatever the screen

-- SGR colors of the heading levels; a level without one is bold, and the
-- first level keeps the heading color of the skin
M.HEADING_COLORS = { [2] = "96", [3] = "92" }

-- a space no line is broken at; written out as a plain space
local NBSP = "\u{00A0}"

-- the combining long stroke that strikes a character through; the terminal
-- has no attribute for it
local STRIKE = "\u{0336}"

-- marks a unit that takes two columns
local WIDE = "\1"

local SGR_ITALIC = "\27[3m"
local SGR_ITALIC_OFF = "\27[23m"
local SGR_COLOR_OFF = "\27[39m"
local LINK_END = "\27]8;;\27\\"

-- The OSC 8 sequence that starts a link to url; bytes that could end the
-- sequence early are dropped and spaces are escaped.
local function link_start(url)
    return "\27]8;;" .. url:gsub("[%c]", ""):gsub(" ", "%%20") .. "\27\\"
end

local BOX_H = "\u{2500}"
local BOX_V = "\u{2502}"
local BOX_X = "\u{253C}"
-- one bullet per nesting level, cycled after the last
local BULLETS = { "\u{2022}", "\u{25E6}", "\u{25AA}" }
local LIST_INDENT = 2  -- columns a nesting level adds

-- the boxes of a task list
local BOX_EMPTY = "\u{2610}"
local BOX_DONE = "\u{2611}"

-- a tab in a code block moves to the next stop
local TAB_WIDTH = 8

------------------------------------------------------------------------
-- Text helpers.  Lengths count characters, not bytes; a stray byte that is
-- not UTF-8 counts as one character and is kept.

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

M.char_width = char_width

local function width(s)
    local n = 0
    for _, ch in ipairs(chars(s)) do
        n = n + char_width(ch)
    end
    return n
end

-- Every line of the document is trimmed, so walk the ends instead of
-- rewriting the string twice.
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

-- style: { bold = b, under = u, italic = i, strike = s, heading = level }.  A space is
-- never overstruck, and the SGR sequences go around the visible characters
-- only, so that a line never starts or ends inside them with a space.
local function styled(s, style)
    local open, close = "", ""
    if style.italic then
        open, close = SGR_ITALIC, SGR_ITALIC_OFF
    end
    local color = style.heading and M.HEADING_COLORS[style.heading]
    if color ~= nil then
        open = open .. "\27[" .. color .. "m"
        close = SGR_COLOR_OFF .. close
    end
    if not (style.bold or style.under or style.heading or style.strike or open ~= "") then
        return s
    end
    local out = {}
    local first, last
    for _, ch in ipairs(chars(s)) do
        if ch == " " then
            out[#out + 1] = style.strike and (ch .. STRIKE) or ch
        else
            if style.heading == 1 then
                out[#out + 1] = ch .. "\b" .. ch .. "\b" .. ch
            elseif (style.bold or style.heading) and style.under then
                out[#out + 1] = "_\b" .. ch .. "\b" .. ch
            elseif style.bold or style.heading then
                out[#out + 1] = ch .. "\b" .. ch
            elseif style.under then
                out[#out + 1] = "_\b" .. ch
            else
                out[#out + 1] = ch
            end
            if style.strike then
                out[#out] = out[#out] .. STRIKE
            end
            first = first or #out
            last = #out
        end
    end
    if first ~= nil and open ~= "" then
        out[first] = open .. out[first]
        out[last] = out[last] .. close
    end
    return table.concat(out)
end

------------------------------------------------------------------------
-- LaTeX in $...$: symbols, \frac, \sqrt, subscripts and superscripts.

local latex = {
    alpha = "\u{03B1}", beta = "\u{03B2}", gamma = "\u{03B3}", delta = "\u{03B4}",
    epsilon = "\u{03B5}", zeta = "\u{03B6}", eta = "\u{03B7}", theta = "\u{03B8}",
    iota = "\u{03B9}", kappa = "\u{03BA}", lambda = "\u{03BB}", mu = "\u{03BC}",
    nu = "\u{03BD}", xi = "\u{03BE}", pi = "\u{03C0}", rho = "\u{03C1}",
    sigma = "\u{03C3}", tau = "\u{03C4}", upsilon = "\u{03C5}", phi = "\u{03C6}",
    chi = "\u{03C7}", psi = "\u{03C8}", omega = "\u{03C9}",
    Gamma = "\u{0393}", Delta = "\u{0394}", Theta = "\u{0398}", Lambda = "\u{039B}",
    Pi = "\u{03A0}", Sigma = "\u{03A3}", Phi = "\u{03A6}", Psi = "\u{03A8}",
    Omega = "\u{03A9}",
    sum = "\u{2211}", prod = "\u{220F}", int = "\u{222B}", infty = "\u{221E}",
    partial = "\u{2202}", nabla = "\u{2207}", pm = "\u{00B1}", times = "\u{00D7}",
    div = "\u{00F7}", cdot = "\u{00B7}",
    leq = "\u{2264}", geq = "\u{2265}", neq = "\u{2260}", approx = "\u{2248}",
    equiv = "\u{2261}", subset = "\u{2282}", supset = "\u{2283}", ["in"] = "\u{2208}",
    forall = "\u{2200}", exists = "\u{2203}",
    to = "\u{2192}", leftarrow = "\u{2190}", rightarrow = "\u{2192}",
    Rightarrow = "\u{21D2}", Leftarrow = "\u{21D0}",
    sqrt = "\u{221A}", lim = "lim",
    cdots = "\u{22EF}", vdots = "\u{22EE}", ddots = "\u{22F1}", ldots = "\u{2026}", dots = "\u{2026}",
    quad = "  ", qquad = "    ",
    -- geometry
    angle = "\u{2220}", measuredangle = "\u{2221}", circ = "\u{2218}", degree = "\u{00B0}",
    parallel = "\u{2225}", nparallel = "\u{2226}", perp = "\u{22A5}", triangle = "\u{25B3}",
    square = "\u{25A1}", cong = "\u{2245}", sim = "\u{223C}", simeq = "\u{2243}",
    -- relations and operators
    ne = "\u{2260}", le = "\u{2264}", ge = "\u{2265}", ll = "\u{226A}", gg = "\u{226B}",
    propto = "\u{221D}", mp = "\u{2213}", ast = "\u{2217}", star = "\u{22C6}", bullet = "\u{2022}",
    oplus = "\u{2295}", otimes = "\u{2297}", mid = "\u{2223}", prime = "\u{2032}",
    -- sets and logic
    notin = "\u{2209}", ni = "\u{220B}", subseteq = "\u{2286}", supseteq = "\u{2287}",
    cup = "\u{222A}", cap = "\u{2229}", setminus = "\u{2216}", emptyset = "\u{2205}",
    varnothing = "\u{2205}", neg = "\u{00AC}", land = "\u{2227}", lor = "\u{2228}",
    wedge = "\u{2227}", vee = "\u{2228}", implies = "\u{21D2}", iff = "\u{21D4}",
    Leftrightarrow = "\u{21D4}", leftrightarrow = "\u{2194}", mapsto = "\u{21A6}",
    uparrow = "\u{2191}", downarrow = "\u{2193}", therefore = "\u{2234}", because = "\u{2235}",
    -- letters
    varepsilon = "\u{03B5}", vartheta = "\u{03D1}", varphi = "\u{03C6}", varrho = "\u{03F1}",
    varsigma = "\u{03C2}", Upsilon = "\u{03A5}", Xi = "\u{039E}", hbar = "\u{210F}",
    ell = "\u{2113}", aleph = "\u{2135}",
    -- functions keep their names
    sin = "sin", cos = "cos", tan = "tan", cot = "cot", log = "log", ln = "ln", exp = "exp",
    min = "min", max = "max",
    -- commands that only change the look of what follows
    left = "", right = "", text = "", mathrm = "", mathbf = "", mathit = "", operatorname = "",
}

local sub_digits = {
    "\u{2080}", "\u{2081}", "\u{2082}", "\u{2083}", "\u{2084}",
    "\u{2085}", "\u{2086}", "\u{2087}", "\u{2088}", "\u{2089}",
}
local sup_digits = {
    "\u{2070}", "\u{00B9}", "\u{00B2}", "\u{00B3}", "\u{2074}",
    "\u{2075}", "\u{2076}", "\u{2077}", "\u{2078}", "\u{2079}",
}

-- The content of the {...} group that starts at pos, and the position after
-- its closing brace.  An unclosed group runs to the end of the string.
local function extract_brace(s, pos)
    if s:sub(pos, pos) ~= "{" then
        return "", pos
    end
    local depth = 0
    for i = pos, #s do
        local ch = s:sub(i, i)
        if ch == "{" then
            depth = depth + 1
        elseif ch == "}" then
            depth = depth - 1
            if depth == 0 then
                return s:sub(pos + 1, i - 1), i + 1
            end
        end
    end
    return s:sub(pos + 1), #s + 1
end

local function paren_unless_word(s)
    if s:match("^%w+$") then
        return s
    end
    return "(" .. s .. ")"
end

local function latex_replace_frac(s)
    local out = {}
    while true do
        local a, b = s:find("\\frac{", 1, true)
        if a == nil then
            break
        end
        out[#out + 1] = s:sub(1, a - 1)
        local num, pos = extract_brace(s, b)
        local den, after = extract_brace(s, pos)
        out[#out + 1] = paren_unless_word(num) .. "/" .. paren_unless_word(den)
        s = s:sub(after)
    end
    out[#out + 1] = s
    return table.concat(out)
end

local function latex_replace_sqrt(s)
    local sym = latex.sqrt
    local out = {}
    while true do
        local a, b = s:find("\\sqrt[%[{]")
        if a == nil then
            break
        end
        out[#out + 1] = s:sub(1, a - 1)
        local pos = b
        if s:sub(pos, pos) == "[" then
            local close = s:find("]", pos, true) or #s
            local idx = s:sub(pos + 1, close - 1)
            local body, after = extract_brace(s, close + 1)
            out[#out + 1] = idx .. sym .. "(" .. body .. ")"
            s = s:sub(after)
        else
            local body, after = extract_brace(s, pos)
            out[#out + 1] = sym .. "(" .. body .. ")"
            s = s:sub(after)
        end
    end
    out[#out + 1] = s
    return table.concat(out)
end

-- symbols written before what they apply to: the space that ends the
-- command name is not shown, \angle ABC is one word
local latex_prefix = {
    angle = true, measuredangle = true, triangle = true, square = true,
    neg = true, partial = true, nabla = true,
}

local function latex_replace_commands(s)
    return (s:gsub("\\(%a+)( ?)", function(cmd, space)
        if latex[cmd] == nil then
            return "\\" .. cmd .. space
        end
        return latex[cmd] .. (latex_prefix[cmd] and "" or space)
    end))
end

local function latex_replace_scripts(s)
    -- a raised \circ is the degree sign
    s = s:gsub("%^{\u{2218}}", "\u{00B0}"):gsub("%^\u{2218}", "\u{00B0}")
    for d = 0, 9 do
        s = s:gsub("_{" .. d .. "}", sub_digits[d + 1])
        s = s:gsub("%^{" .. d .. "}", sup_digits[d + 1])
    end
    for d = 0, 9 do
        s = s:gsub("_" .. d, sub_digits[d + 1])
        s = s:gsub("%^" .. d, sup_digits[d + 1])
    end
    while true do
        local a = s:find("[_^]{")
        if a == nil then
            break
        end
        local body, after = extract_brace(s, a + 1)
        s = s:sub(1, a) .. "(" .. body .. ")" .. s:sub(after)
    end
    return s
end

-- \vec{v} and its kin: the mark goes over every character of the group
local latex_accents = {
    vec = "\u{20D7}", overline = "\u{0305}", bar = "\u{0304}", hat = "\u{0302}",
    tilde = "\u{0303}", dot = "\u{0307}", ddot = "\u{0308}", check = "\u{030C}",
    breve = "\u{0306}", acute = "\u{0301}", grave = "\u{0300}", underline = "\u{0332}",
}

local function latex_replace_accents(s)
    local out = {}

    while true do
        local a, b, cmd = s:find("\\(%a+){")
        if a == nil or latex_accents[cmd] == nil then
            break
        end
        local body, after = extract_brace(s, b)

        out[#out + 1] = s:sub(1, a - 1)
        for _, ch in ipairs(chars(body)) do
            out[#out + 1] = ch .. latex_accents[cmd]
        end
        s = s:sub(after)
    end
    out[#out + 1] = s
    return table.concat(out)
end

-- The limits of a big operator go after it in brackets: the terminal has no
-- room above and below the sign.
local latex_big = {
    ["\u{2211}"] = true, ["\u{220F}"] = true, ["\u{222B}"] = true,
    ["\u{22C3}"] = true, ["\u{22C2}"] = true, ["lim"] = true,
}

local function latex_replace_limits(s)
    local out = {}
    local i = 1

    while i <= #s do
        local matched = false

        for sign, _ in pairs(latex_big) do
            if s:sub(i, i + #sign - 1) == sign then
                local rest = s:sub(i + #sign)
                local from, after = nil, nil

                if rest:sub(1, 2) == "_{" then
                    from, after = extract_brace(rest, 2)
                end
                if from ~= nil then
                    local to = nil

                    if rest:sub(after, after + 1) == "^{" then
                        to, after = extract_brace(rest, after + 1)
                    end
                    out[#out + 1] = sign .. "(" .. from .. (to ~= nil and (".." .. to) or "") .. ")"
                    i = i + #sign + after - 1
                    matched = true
                    break
                end
            end
        end
        if not matched then
            out[#out + 1] = s:sub(i, i)
            i = i + 1
        end
    end
    return table.concat(out)
end

local function render_math(math)
    math = latex_replace_frac(math)
    math = latex_replace_sqrt(math)
    math = latex_replace_accents(math)
    math = latex_replace_commands(math)
    math = latex_replace_limits(math)
    math = latex_replace_scripts(math)
    return (math:gsub("[{}]", ""))
end

------------------------------------------------------------------------
-- Environments in display math: matrices and the like become a block of
-- lines, the columns lined up, the brackets built from Unicode pieces.

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

------------------------------------------------------------------------
-- Inline markup.  The line is cut into tokens: text, code, a formula, a
-- link, or a run of * or _ that may open or close emphasis.  The runs are
-- then paired the way CommonMark pairs them, and the text is written out
-- with the styles that are open at each point.

local entities = {
    amp = "&", lt = "<", gt = ">", quot = "\"", apos = "'", nbsp = " ",
    copy = "\u{00A9}", reg = "\u{00AE}", trade = "\u{2122}", hellip = "\u{2026}",
    mdash = "\u{2014}", ndash = "\u{2013}", laquo = "\u{00AB}", raquo = "\u{00BB}",
    bull = "\u{2022}", middot = "\u{00B7}", times = "\u{00D7}", deg = "\u{00B0}",
    larr = "\u{2190}", rarr = "\u{2192}", lsquo = "\u{2018}", rsquo = "\u{2019}",
    ldquo = "\u{201C}", rdquo = "\u{201D}",
}

local function is_punct(ch)
    return ch ~= "" and ch:match("^%p$") ~= nil
end

local function is_space(ch)
    return ch == "" or ch:match("^%s$") ~= nil
end

-- Whether a run of * or _ can open or close emphasis, from what is around
-- it (CommonMark's left- and right-flanking rules).
local function flanking(before, after, ch)
    local left = not is_space(after) and (not is_punct(after) or is_space(before) or is_punct(before))
    local right = not is_space(before) and (not is_punct(before) or is_space(after) or is_punct(after))
    if ch == "_" then
        return left and (not right or is_punct(before)), right and (not left or is_punct(after))
    end
    return left, right
end

local tokenize
local pair_emphasis

-- What the document defines for the inline pass: link reference definitions
-- by label, footnotes by label and the numbers footnotes get in the order
-- they are first referenced.  Set by M.render for one document.
local doc = { refs = {}, notes = {}, note_defs = {}, note_order = {}, note_number = {} }

local function normalize_label(label)
    return (trim(label):gsub("%s+", " "):lower())
end

-- The number of the footnote label refers to, given on its first reference,
-- or nil if the document has no such footnote.
local function footnote_number(label)
    local key = normalize_label(label)
    if doc.notes[key] == nil then
        return nil
    end
    if doc.note_number[key] == nil then
        doc.note_order[#doc.note_order + 1] = key
        doc.note_number[key] = #doc.note_order
    end
    return doc.note_number[key]
end

-- The tokens of s.  Each is { kind, text } for plain text, { "code", text },
-- { "link", tokens, url }, or { "delim", ch, count, can_open, can_close }.
tokenize = function(s)
    local tokens = {}
    local text = {}
    local n = #s
    local i = 1

    local function flush()
        if #text > 0 then
            tokens[#tokens + 1] = { "text", table.concat(text) }
            text = {}
        end
    end

    while i <= n do
        local ch = s:sub(i, i)
        if ch == "\\" and is_punct(s:sub(i + 1, i + 1)) then
            text[#text + 1] = s:sub(i + 1, i + 1)
            i = i + 2
        elseif ch == "`" then
            local _, e = s:find("^`+", i)
            local fence = s:sub(i, e)
            local a, b = s:find(fence, e + 1, true)
            while a ~= nil and s:sub(b + 1, b + 1) == "`" do
                local _, e2 = s:find("^`+", b + 1)
                a, b = s:find(fence, e2 + 1, true)
            end
            if a == nil then
                text[#text + 1] = fence
                i = e + 1
            else
                local code = s:sub(e + 1, a - 1)
                if code:match("^ .* $") and code:match("%S") then
                    code = code:sub(2, -2)
                end
                flush()
                tokens[#tokens + 1] = { "code", code }
                i = b + 1
            end
        elseif ch == "$" then
            local delim = s:sub(i, i + 1) == "$$" and "$$" or "$"
            local a, b = find_math_end(s, i + #delim, delim)
            if a == nil then
                text[#text + 1] = ch
                i = i + 1
            else
                -- a formula is not broken across lines
                local formula = s:sub(i + #delim, a - 1)
                local rendered = formula:find("\\begin{", 1, true) and render_math_inline(formula)
                    or render_math(formula)

                text[#text + 1] = (rendered:gsub(" ", NBSP))
                i = b + 1
            end
        elseif ch == "&" then
            local name, e = s:match("^&(#?%w+);()", i)
            local value
            if name ~= nil then
                if name:match("^#[xX]%x+$") then
                    value = utf8.char(tonumber(name:sub(3), 16))
                elseif name:match("^#%d+$") then
                    value = utf8.char(tonumber(name:sub(2)))
                else
                    value = entities[name]
                end
            end
            if value ~= nil then
                text[#text + 1] = value
                i = e
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "<" and s:match("^<[iI][mM][gG][%s>]", i) then
            -- a picture of html: named, not drawn, like a markdown one
            local tag, e = s:match("^(<[^<>]*>)()", i)

            if tag == nil then
                text[#text + 1] = ch
                i = i + 1
            else
                local src = tag:match("[sS][rR][cC]%s*=%s*\"([^\"]*)\"")
                    or tag:match("[sS][rR][cC]%s*=%s*'([^']*)'")
                local alt = tag:match("[aA][lL][tT]%s*=%s*\"([^\"]*)\"")
                    or tag:match("[aA][lL][tT]%s*=%s*'([^']*)'")

                text[#text + 1] = "[" .. (alt or "") .. "]"
                if src ~= nil and src ~= "" then
                    text[#text + 1] = " <" .. src .. ">"
                end
                i = e
            end
        elseif ch == "<" then
            local url, e = s:match("^<(%a[%w+.-]*:[^%s<>]*)>()", i)
            if url == nil then
                url, e = s:match("^<([%w.+-]+@[%w.-]+)>()", i)
            end
            local tag_end = url == nil and (s:match("^<!%-%-.-%-%->()", i) or s:match("^<[!/]?%a[^<>]*>()", i))
                or nil
            if url ~= nil then
                flush()
                local target = url:find(":", 1, true) and url or "mailto:" .. url
                tokens[#tokens + 1] = { "link", { { "text", url } }, target }
                i = e
            elseif tag_end ~= nil then
                if s:sub(i, tag_end - 1):match("^<[Bb][Rr]%s*/?>$") then
                    text[#text + 1] = "\n"
                end
                i = tag_end
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "!" and s:sub(i + 1, i + 1) == "[" then
            -- a picture is not drawn: its text and where it comes from are
            local alt, target, e = s:match("^!%[([^%]]*)%]%(([^%s)]*)[^)]*%)()", i)
            local ref

            if alt == nil then
                alt, ref, e = s:match("^!%[([^%]]*)%]%[([^%]]*)%]()", i)
                if alt ~= nil then
                    target = doc.refs[normalize_label(ref ~= "" and ref or alt)]
                end
            end
            if alt == nil then
                alt, e = s:match("^!%[([^%]]*)%]()", i)
                if alt ~= nil then
                    target = doc.refs[normalize_label(alt)]
                end
            end
            if alt ~= nil then
                text[#text + 1] = "[" .. alt .. "]"
                if target ~= nil and target ~= "" and target ~= alt then
                    text[#text + 1] = " <" .. target .. ">"
                end
                i = e
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "[" then
            local note_label, note_end = s:match("^%[%^([^%]%s]+)%]()", i)
            local number = note_label ~= nil and footnote_number(note_label) or nil
            if number ~= nil then
                text[#text + 1] = "[" .. number .. "]"
                i = note_end
            else
                local label, e = s:match("^%[([^%[%]]*)%]()", i)
                local url
                if label ~= nil then
                    local target, e2 = s:match("^%(([^%s)]*)[^)]*%)()", e)
                    if target ~= nil then
                        url = target
                        e = e2
                    else
                        -- [text][ref], [text][] or a bare [ref]
                        local ref, ref_end = s:match("^%[([^%]]*)%]()", e)
                        if ref ~= nil then
                            e = ref_end
                            url = doc.refs[normalize_label(ref ~= "" and ref or label)]
                        else
                            url = doc.refs[normalize_label(label)]
                        end
                    end
                end
                if label ~= nil and label ~= "" and (url ~= nil or e > i + #label + 2) then
                    flush()
                    local label_tokens = tokenize(label)
                    pair_emphasis(label_tokens)
                    tokens[#tokens + 1] = { "link", label_tokens, url }
                    i = e
                else
                    text[#text + 1] = ch
                    i = i + 1
                end
            end
        elseif ch == "*" or ch == "_" or ch == "~" then
            local _, e = s:find("^" .. (ch == "*" and "%*+" or ch == "_" and "_+" or "~+"), i)
            local before = s:sub(i - 1, i - 1)
            local after = s:sub(e + 1, e + 1)
            local can_open, can_close = flanking(before, after, ch)
            if can_open or can_close then
                flush()
                tokens[#tokens + 1] = { "delim", ch, e - i + 1, can_open, can_close }
            else
                text[#text + 1] = s:sub(i, e)
            end
            i = e + 1
        else
            -- Keep ordinary text together, including all bytes of UTF-8 characters.
            local e = s:find("[\\`$&<!%[*_~]", i + 1) or (n + 1)
            text[#text + 1] = s:sub(i, e - 1)
            i = e
        end
    end
    flush()
    return tokens
end

-- Pair the delimiter runs: each closer takes the nearest opener of the same
-- character; two or more on both sides make strong emphasis, one makes
-- emphasis.  Runs left unpaired are text.
pair_emphasis = function(tokens)
    for c = 1, #tokens do
        local closer = tokens[c]
        while closer[1] == "delim" and closer[5] and closer[3] > 0 do
            local opener
            for o = c - 1, 1, -1 do
                local t = tokens[o]
                if t[1] == "delim" and t[2] == closer[2] and t[4] and t[3] > 0 then
                    local odd = (t[5] or closer[4]) and (t[3] + closer[3]) % 3 == 0
                        and not (t[3] % 3 == 0 and closer[3] % 3 == 0)
                    if not odd then
                        opener = t
                        break
                    end
                end
            end
            if opener == nil then
                break
            end
            local use
            if closer[2] == "~" then
                use = 3  -- strike: one or two tildes, all of the run at once
                opener[3], closer[3] = 1, 1
            else
                use = (opener[3] >= 2 and closer[3] >= 2) and 2 or 1
            end
            opener[3] = opener[3] - use
            closer[3] = closer[3] - use
            opener.opens = opener.opens or {}
            closer.closes = closer.closes or {}
            table.insert(opener.opens, use)
            table.insert(closer.closes, 1, use)
        end
    end
end

local render_tokens

render_tokens = function(tokens, style, out)
    local bold, italic, strike = 0, 0, 0
    local function current()
        return {
            bold = style.bold or bold > 0,
            under = style.under,
            italic = style.italic or italic > 0,
            strike = style.strike or strike > 0,
            heading = style.heading,
        }
    end
    for _, t in ipairs(tokens) do
        if t[1] == "text" then
            out[#out + 1] = styled(t[2], current())
        elseif t[1] == "code" then
            out[#out + 1] = styled(t[2], { under = true, heading = style.heading })
        elseif t[1] == "link" and (t[3] == nil or t[3] == "") then
            -- a link without a target is only underlined
            local s = current()
            s.under = true
            render_tokens(t[2], s, out)
        elseif t[1] == "link" then
            -- the viewer underlines the text of an OSC 8 link itself
            local label = {}
            render_tokens(t[2], current(), label)
            local lead, body, tail = table.concat(label):match("^( *)(.-)( *)$")
            out[#out + 1] = lead .. link_start(t[3]) .. body .. LINK_END .. tail
            local plain = {}
            render_tokens(t[2], {}, plain)
            local shown = t[3]:gsub("^mailto:", "")
            if table.concat(plain) ~= shown then
                out[#out + 1] = styled(" <" .. t[3] .. ">", current())
            end
        else
            if t.closes then
                for _, use in ipairs(t.closes) do
                    if use == 3 then
                        strike = strike - 1
                    elseif use == 2 then
                        bold = bold - 1
                    else
                        italic = italic - 1
                    end
                end
            end
            if t[3] > 0 then
                out[#out + 1] = styled(t[2]:rep(t[3]), current())
            end
            if t.opens then
                for _, use in ipairs(t.opens) do
                    if use == 3 then
                        strike = strike + 1
                    elseif use == 2 then
                        bold = bold + 1
                    else
                        italic = italic + 1
                    end
                end
            end
        end
    end
end

local function inline(s, style)
    local tokens = tokenize(s)
    pair_emphasis(tokens)
    local out = {}
    render_tokens(tokens, style or {}, out)
    return table.concat(out)
end

M.inline = inline

------------------------------------------------------------------------
-- Code blocks: the language of the fence, colored by the syntax rules of
-- the editor through mc.syntax.scan.

-- What a fence writes, turned into the name of a file: the rules of the
-- editor are chosen by name, and the name of a language is usually its
-- extension.  Only the ones that differ are listed.
M.CODE_LANGUAGES = {
    bash = "sh", zsh = "sh", shell = "sh", console = "sh",
    ["c++"] = "cpp", cxx = "cpp", cc = "cpp", hpp = "h",
    javascript = "js", node = "js", typescript = "ts",
    python = "py", ruby = "rb", rust = "rs", kotlin = "kt", perl = "pl",
    markdown = "md", yml = "yaml", patch = "diff", conf = "ini",
}

-- Languages whose rules are chosen by a whole name, not by an extension.
M.CODE_FILENAMES = {
    make = "Makefile", makefile = "Makefile", cmake = "CMakeLists.txt",
    dockerfile = "Dockerfile",
}

local SGR_COLORS = {
    black = 30, red = 31, green = 32, brown = 33, yellow = 33, blue = 34,
    magenta = 35, cyan = 36, lightgray = 37, gray = 90, brightred = 91,
    brightgreen = 92, brightbrown = 93, brightblue = 94, brightmagenta = 95,
    brightcyan = 96, white = 97,
}

local SGR_ATTRS = { bold = "1", italic = "3", underline = "4", reverse = "7" }

-- The SGR sequence of one color of a rule set, "" for the plain one.
local function sgr_of_color(color)
    local codes = {}

    if color ~= nil then
        if color.attrs ~= nil then
            for attr in color.attrs:gmatch("[^+]+") do
                if SGR_ATTRS[attr] ~= nil then
                    codes[#codes + 1] = SGR_ATTRS[attr]
                end
            end
        end
        if color.fg ~= nil and SGR_COLORS[color.fg] ~= nil then
            codes[#codes + 1] = tostring(SGR_COLORS[color.fg])
        end
    end
    if #codes == 0 then
        return ""
    end
    return "\27[" .. table.concat(codes, ";") .. "m"
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
            local fill = TAB_WIDTH - column % TAB_WIDTH

            out[#out + 1] = (" "):rep(fill)
            column = column + fill
        else
            out[#out + 1] = ch
            column = column + char_width(ch)
        end
    end
    return table.concat(out)
end

-- The scan of a code block, or nil when there are no rules for it or no mc
-- to ask (the renderer also runs outside it).  The language of the fence is
-- given as the name of a file, the way the rules are chosen for a real one;
-- a fence without a language leaves the choice to the first line of the code,
-- which is how a shebang is recognized.
local function scan_code(code, language)
    if mc == nil or mc.syntax == nil or mc.syntax.scan == nil then
        return nil
    end
    if language == nil or language == "" then
        return mc.syntax.scan(code)
    end
    local key = language:lower()
    local filename = M.CODE_FILENAMES[key] or ("code." .. (M.CODE_LANGUAGES[key] or key))
    return mc.syntax.scan(code, { filename = filename })
end

-- A mermaid diagram is drawn, not shown as code, when it is one the drawing
-- knows; anything else stays a code block.
local function mermaid_lines(code, language, out, width_limit)
    if language == nil or language:lower() ~= "mermaid" then
        return false
    end
    local drawn = mermaid.render(code, width_limit ~= nil and width_limit - 4 or nil)

    if drawn == nil then
        return false
    end
    for _, line in ipairs(drawn) do
        out[#out + 1] = line == "" and "" or ("    " .. line)
    end
    return true
end

-- The lines of a code block, colored where the rules say so.  Each line
-- opens the color it starts in and closes it at its end, because the viewer
-- may start reading at any line.
local function code_lines(code, language, out, width_limit)
    if mermaid_lines(code, language, out, width_limit) then
        return
    end
    local scan = scan_code(code, language)
    local colored = {}
    local pos = 1

    if scan == nil then
        for line in (code .. "\n"):gmatch("(.-)\n") do
            out[#out + 1] = "    " .. expand_tabs(line)
        end
        if code:sub(-1) == "\n" then
            out[#out] = nil
        end
        return
    end

    for _, run in ipairs(scan.runs) do
        local sgr = sgr_of_color(scan.colors[run.color])
        local text = code:sub(run.offset, run.offset + run.length - 1)

        for piece, eol in (text .. "\0"):gmatch("([^\n]*)(\n?)") do
            if piece ~= "" then
                colored[#colored + 1] = sgr ~= "" and (sgr .. piece:gsub("%z", "") .. "\27[0m")
                    or piece:gsub("%z", "")
            end
            if eol == "\n" then
                colored[#colored + 1] = "\n"
            end
        end
        pos = run.offset + run.length
    end
    if pos <= #code then
        colored[#colored + 1] = code:sub(pos)
    end

    for line in (table.concat(colored) .. "\n"):gmatch("(.-)\n") do
        out[#out + 1] = "    " .. expand_tabs(line)
    end
    if code:sub(-1) == "\n" then
        out[#out] = nil
    end
end

------------------------------------------------------------------------
-- Tables.

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

local function render_table(lines, out, width_limit)
    local rows = {}
    local align = {}
    local maxc = 0
    for _, line in ipairs(lines) do
        local fields = split_row(line)
        if is_table_sep(line) then
            for c, f in ipairs(fields) do
                if f:match("^:%-+:$") then
                    align[c] = "center"
                elseif f:match(":$") then
                    align[c] = "right"
                end
            end
        else
            rows[#rows + 1] = fields
            if #fields > maxc then
                maxc = #fields
            end
        end
    end
    if #rows == 0 then
        return
    end

    -- Each column as wide as its widest cell; when that does not fit the
    -- screen, the columns that take more than a fair share of what is left
    -- give up width equally, and their cells are wrapped to it.
    local units = {}
    local colw = {}
    for c = 1, maxc do
        colw[c] = 0
    end
    for r, row in ipairs(rows) do
        units[r] = {}
        for c = 1, maxc do
            units[r][c] = units_of(inline(row[c] or "", { bold = r == 1 }))
            if units_width(units[r][c]) > colw[c] then
                colw[c] = units_width(units[r][c])
            end
        end
    end
    local remaining = width_limit - (maxc - 1) * 3
    local total = 0
    for c = 1, maxc do
        total = total + colw[c]
    end
    if total > remaining then
        local flex = {}
        for c = 1, maxc do
            flex[#flex + 1] = c
        end
        local settled = true
        while settled and #flex > 0 do
            local share = remaining // #flex
            settled = false
            for k = #flex, 1, -1 do
                local c = flex[k]
                if colw[c] <= share then
                    remaining = remaining - colw[c]
                    table.remove(flex, k)
                    settled = true
                end
            end
        end
        for k, c in ipairs(flex) do
            local share = remaining // #flex + (k <= remaining % #flex and 1 or 0)
            colw[c] = math.max(share, M.MIN_COLUMN)
        end
    end

    local cells = {}
    local cell_sgr = {}
    for r = 1, #rows do
        cells[r] = {}
        cell_sgr[r] = {}
        for c = 1, maxc do
            local u = units[r][c]
            cells[r][c] = units_width(u) > colw[c] and wrap_units(u, colw[c]) or { u }
            cell_sgr[r][c] = {}
        end
    end

    local rule = {}
    for c = 1, maxc do
        rule[c] = BOX_H:rep(colw[c])
    end
    rule = table.concat(rule, BOX_H .. BOX_X .. BOX_H)

    for r = 1, #rows do
        local height = 1
        for c = 1, maxc do
            if #cells[r][c] > height then
                height = #cells[r][c]
            end
        end
        for k = 1, height do
            local parts = {}
            for c = 1, maxc do
                local cell = cells[r][c][k] or {}
                local pad = colw[c] - units_width(cell)
                local left, right = 0, pad
                if align[c] == "right" then
                    left, right = pad, 0
                elseif align[c] == "center" then
                    left = pad // 2
                    right = pad - left
                end
                parts[c] = (" "):rep(left) .. sgr_line(cell, cell_sgr[r][c]) .. (" "):rep(right)
            end
            out[#out + 1] = table.concat(parts, " " .. BOX_V .. " ")
        end
        out[#out + 1] = rule
    end
end

------------------------------------------------------------------------
-- The document.

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

-- Whether the line opens a block of its own, which ends the paragraph
-- before it.
local function starts_block(line, next_line)
    return is_blank(line) or fence_of(line) ~= nil or is_hr(line) or is_atx_heading(line)
        or list_item(line) ~= nil or definition_item(line) ~= nil or line:match("^%s*>") ~= nil
        or (line:find("|", 1, true) ~= nil and next_line ~= nil and is_table_sep(next_line))
        or line:match("^%s*%$%$.*\\begin{") ~= nil
end

-- The lines of a paragraph, list item or quote joined into one flow and
-- wrapped to the width: the first line behind prefix, the rest behind as
-- many spaces.  A line that ends in two spaces or a backslash keeps its
-- break, and so does a <br>.
local function flow(pieces, prefix, width_limit, out)
    local room = width_limit - width(prefix)
    if room < 10 then
        room = 10
    end
    local hanging = (" "):rep(width(prefix))
    local text = {}
    local hard = {}
    for _, piece in ipairs(pieces) do
        local body = piece:gsub("%s+$", "")
        if piece:match("  $") or body:match("\\$") then
            hard[#text + 1] = true
            body = body:gsub("\\$", "")
        end
        text[#text + 1] = body
    end
    local logical = {}
    local cur = {}
    for k, body in ipairs(text) do
        cur[#cur + 1] = body
        if hard[k] then
            logical[#logical + 1] = table.concat(cur, " ")
            cur = {}
        end
    end
    if #cur > 0 then
        logical[#logical + 1] = table.concat(cur, " ")
    end
    local sgr = {}
    for _, s in ipairs(logical) do
        for rendered in (inline(s, {}) .. "\n"):gmatch("(.-)\n") do
            for _, seg in ipairs(wrap_units(units_of(rendered), room)) do
                out[#out + 1] = prefix .. sgr_line(seg, sgr)
                prefix = hanging
            end
        end
    end
end

-- The html a document written for the web carries, turned into the markdown
-- that says the same: a heading, a list item, a rule, a row of cells.  What
-- is left of a tag the inline pass drops.
local function html_line(line)
    local body = trim(line)
    local tag, text

    tag, text = body:match("^<[hH](%d)[^<>]*>(.*)$")
    if tag ~= nil then
        return ("#"):rep(math.min(tonumber(tag), 6)) .. " " .. text:gsub("</[hH]%d>%s*$", "")
    end
    if body:match("^<[hH][rR]%s*/?>$") then
        return "---"
    end
    text = body:match("^<[lL][iI][^<>]*>(.*)$")
    if text ~= nil then
        return "- " .. text:gsub("</[lL][iI]>%s*$", "")
    end
    text = body:match("^<[sS][uU][mM][mM][aA][rR][yY][^<>]*>(.*)$")
    if text ~= nil then
        return "**" .. text:gsub("</[sS][uU][mM][mM][aA][rR][yY]>%s*$", "") .. "**"
    end
    if body:match("^<[tT][rR][^<>]*>") then
        -- the cells of a row become the cells of a markdown table
        local cells = {}

        for cell in body:gmatch("<[tTdDhH]+[^<>]*>([^<]*)") do
            if trim(cell) ~= "" then
                cells[#cells + 1] = trim(cell)
            end
        end
        if #cells > 0 then
            return "|" .. table.concat(cells, "|") .. "|", #cells
        end
    end
    -- a line of tags alone says nothing
    if body ~= "" and body:gsub("<[^<>]*>", ""):match("^%s*$") then
        return nil
    end
    return line
end

-- Take the link reference definitions and the footnotes out of the lines,
-- into doc, and drop the lines that hold only an HTML comment; code blocks
-- are left alone.  A footnote goes on over the lines
-- indented under it.
local function collect_definitions(lines)
    local kept = {}
    local fence
    local note
    local dropped = false -- a definition was taken out since the last text
    local html_table = false
    doc = { refs = {}, notes = {}, note_defs = {}, note_order = {}, note_number = {} }

    -- a blank line left over where a definition was taken out is not kept
    local function keep(line)
        if not is_blank(line) then
            dropped = false
        elseif dropped and (#kept == 0 or is_blank(kept[#kept])) then
            return
        end
        kept[#kept + 1] = line
    end

    for _, line in ipairs(lines) do
        local label, rest

        if fence == nil and line:find("<", 1, true) ~= nil then
            local converted, cells = html_line(line)

            if converted == nil then
                dropped = true
                goto continue
            end
            line = converted
            if cells ~= nil and not html_table then
                -- the first row of a table is its header; the line that says
                -- so is what the markdown table needs next
                html_table = true
                keep(line)
                line = ("|---"):rep(cells) .. "|"
            end
        elseif html_table and trim(line) == "" then
            html_table = false
        end
        if fence == nil then
            label, rest = line:match("^ ? ? ?%[%^([^%]%s]+)%]:%s*(.*)$")
        end
        if fence == nil and label == nil and line:match("^%s*<!%-%-.-%-%->%s*$") then
            note = nil
            dropped = true
        elseif fence ~= nil then
            local close = line:match("^ ? ? ?([`~]+)%s*$")
            if close ~= nil and close:sub(1, 1) == fence:sub(1, 1) and #close >= #fence then
                fence = nil
            end
            keep(line)
        elseif label ~= nil then
            local key = normalize_label(label)
            note = { rest }
            dropped = true
            if doc.notes[key] == nil then
                doc.notes[key] = note
                doc.note_defs[#doc.note_defs + 1] = key
            end
        elseif note ~= nil and not is_blank(line) and (line:match("^    ") or line:match("^\t")) then
            note[#note + 1] = trim(line)
        else
            note = nil
            local ref, url = line:match("^ ? ? ?%[([^%]^][^%]]*)%]:%s*<?([^%s>]+)>?")
            if ref ~= nil then
                local key = normalize_label(ref)
                dropped = true
                if doc.refs[key] == nil then
                    doc.refs[key] = url
                end
            else
                fence = fence_of(line)
                keep(line)
            end
        end
        ::continue::
    end
    return kept
end

-- The footnotes at the end, numbered as they were referenced; those never
-- referenced follow in the order they were written.
local function render_footnotes(width_limit, out)
    if #doc.note_defs == 0 then
        return
    end
    for _, key in ipairs(doc.note_defs) do
        footnote_number(key)
    end
    while out[#out] == "" do
        out[#out] = nil
    end
    out[#out + 1] = ""
    out[#out + 1] = BOX_H:rep(math.min(20, width_limit))
    local k = 1
    while k <= #doc.note_order do
        flow(doc.notes[doc.note_order[k]], "[" .. k .. "] ", width_limit, out)
        k = k + 1
    end
end

local function render_document(text, opts, emit)
    local width_limit = opts and opts.width or M.DEFAULT_WIDTH
    local lines = collect_definitions(join_display_math(split_lines(text)))
    local out = {}
    local i = 1
    local emitted = false
    local prev_blank = true
    local prev_list = false
    local list_levels = {}  -- the indents of the lists that are open
    local list_hanging = ""  -- what a continuation line of the last item is indented by

    while i <= #lines do
        local line = lines[i]
        local next_line = lines[i + 1]
        local fence = fence_of(line)
        local blank = is_blank(line)
        local was_list = prev_list
        prev_list = blank and was_list
        if not blank and not was_list then
            list_levels = {}
        end

        if fence ~= nil then
            local fence_char = fence:sub(1, 1)
            local language = trim(line:match("^ ? ? ?[`~]+(.*)$") or ""):match("^([%w+#._-]*)")
            local code = {}

            i = i + 1
            while i <= #lines do
                local close = lines[i]:match("^ ? ? ?(" .. (fence_char == "`" and "```+" or "~~~+") .. ")%s*$")
                if close ~= nil and #close >= #fence then
                    break
                end
                code[#code + 1] = lines[i]
                i = i + 1
            end
            code_lines(table.concat(code, "\n"), language, out, width_limit)
            i = i + 1
        elseif line:find("<!--", 1, true) and not line:find("-->", 1, true) then
            while i <= #lines and not lines[i]:find("-->", 1, true) do
                i = i + 1
            end
            i = i + 1
        elseif blank then
            out[#out + 1] = ""
            i = i + 1
        elseif prev_blank and not was_list and (line:match("^    ") or line:match("^\t")) then
            while i <= #lines and (lines[i]:match("^    ") or lines[i]:match("^\t") or is_blank(lines[i])) do
                if is_blank(lines[i]) and not (lines[i + 1] and (lines[i + 1]:match("^    ") or lines[i + 1]:match("^\t"))) then
                    break
                end
                out[#out + 1] = expand_tabs(lines[i])
                i = i + 1
            end
        elseif math_block_of(line, width_limit) ~= nil then
            for _, l in ipairs(math_block_of(line, width_limit)) do
                out[#out + 1] = l
            end
            i = i + 1
        elseif line:find("|", 1, true) and next_line ~= nil and is_table_sep(next_line) then
            local rows = { line, next_line }
            i = i + 2
            while i <= #lines and lines[i]:find("|", 1, true) and not is_blank(lines[i]) do
                rows[#rows + 1] = lines[i]
                i = i + 1
            end
            render_table(rows, out, width_limit)
        elseif definition_item(line) ~= nil then
            -- the meaning is indented under the term it belongs to
            local pieces = { definition_item(line) }

            i = i + 1
            while i <= #lines and not starts_block(lines[i], lines[i + 1]) do
                pieces[#pieces + 1] = trim(lines[i])
                i = i + 1
            end
            flow(pieces, "    ", width_limit, out)
        elseif is_hr(line) then
            out[#out + 1] = BOX_H:rep(width_limit)
            i = i + 1
        elseif is_atx_heading(line) then
            local hashes, rest = line:match("^ ? ? ?(#+)%s*(.-)%s*$")
            rest = rest:gsub("%s+#+$", ""):gsub("^#+$", "")
            out[#out + 1] = inline(rest, { heading = math.min(#hashes, 6) })
            i = i + 1
        elseif next_line ~= nil and not was_list and list_item(line) == nil
            and (next_line:match("^ ? ? ?=+%s*$") or next_line:match("^ ? ? ?%-+%s*$")) then
            out[#out + 1] = inline(trim(line), { heading = next_line:match("=") and 1 or 2 })
            i = i + 2
        else
            local quotes = 0
            local rest = line
            while true do
                local after = rest:match("^%s*> ?(.*)$")
                if after == nil then
                    break
                end
                quotes = quotes + 1
                rest = after
            end
            local prefix = (BOX_V .. " "):rep(quotes)
            local indent, marker, item = list_item(rest)
            if indent ~= nil then
                local depth = #indent
                local level

                -- the indent of the source says how deep the item is; the
                -- output indents every level by the same amount
                while #list_levels > 0 and list_levels[#list_levels] > depth do
                    list_levels[#list_levels] = nil
                end
                if #list_levels == 0 or list_levels[#list_levels] < depth then
                    list_levels[#list_levels + 1] = depth
                end
                level = #list_levels

                local bullet = marker:match("^%d") and marker
                    or BULLETS[(level - 1) % #BULLETS + 1]
                local task, task_text = item:match("^%[([ xX])%]%s+(.*)$")

                -- a task list: the box takes the place of the bullet
                if task ~= nil and not marker:match("^%d") then
                    bullet = task == " " and BOX_EMPTY or BOX_DONE
                    item = task_text
                end

                prefix = prefix .. (" "):rep((level - 1) * LIST_INDENT) .. bullet .. " "
                list_hanging = (" "):rep(width(prefix) - quotes * 2)
                rest = item
                prev_list = true
            elseif was_list and rest:match("^%s") then
                prefix = prefix .. list_hanging
                rest = rest:gsub("^%s+", "")
                prev_list = true
            end
            local pieces = { rest }
            i = i + 1
            while i <= #lines and not starts_block(lines[i], lines[i + 1]) do
                local more = lines[i]
                for _ = 1, quotes do
                    more = more:match("^%s*> ?(.*)$") or more
                end
                if quotes == 0 and more:match("^%s*>") then
                    break
                end
                pieces[#pieces + 1] = more:gsub("^%s+", "")
                i = i + 1
            end
            flow(pieces, prefix, width_limit, out)
        end
        prev_blank = blank
        if emit ~= nil then
            -- Footnotes remove trailing blank lines, so hold those until the next block.
            local last = #out
            while last > 0 and out[last] == "" do
                last = last - 1
            end
            if last > 0 then
                local chunk = (table.concat(out, "\n", 1, last):gsub(NBSP, " ")) .. "\n"
                local pending = {}
                for k = last + 1, #out do
                    pending[#pending + 1] = out[k]
                end
                out = pending
                emitted = true
                emit(chunk)
            end
        end
    end
    render_footnotes(width_limit, out)
    local tail = (table.concat(out, "\n"):gsub(NBSP, " ")) .. "\n"
    if emit == nil then
        return tail
    end
    if #out > 0 or not emitted then
        emit(tail)
    end
end

function M.render(text, opts)
    return render_document(text, opts)
end

-- One complete Markdown block per call, nil at EOF. The preliminary pass collects
-- forward references before any output is emitted. Each iterator owns its document
-- state, so a resize or another viewer can render between two calls.
function M.blocks(text, opts)
    local state
    local co = coroutine.create(function()
        render_document(text, opts, coroutine.yield)
    end)
    return function()
        if coroutine.status(co) == "dead" then
            return nil
        end
        local previous = doc
        if state ~= nil then
            doc = state
        end
        local ok, chunk = coroutine.resume(co)
        state = doc
        doc = previous
        if not ok then
            error(chunk, 0)
        end
        return chunk
    end
end

return M
