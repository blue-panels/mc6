-- Markdown to the nroff-style text the viewer paints: overstruck letters
-- for headings and bold, underscore overstrikes for italic, code and links.
-- One pass over the lines for the blocks, one tokenizing pass per line for
-- the inline markup; nothing is scanned twice.

local M = {}

M.MIN_COLUMN = 8     -- a table column is never squeezed narrower than this
M.DEFAULT_WIDTH = 80 -- the screen width when the caller names none
M.MAX_WIDTH = 120    -- text is never flowed wider than this, whatever the screen

local BOX_H = "\u{2500}"
local BOX_V = "\u{2502}"
local BOX_X = "\u{253C}"
local BULLET = "\u{2022}"

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

local function width(s)
    return #chars(s)
end

local function trim(s)
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

-- style: { bold = b, under = u, heading = h }; a space is never overstruck
local function styled(s, style)
    if not (style.bold or style.under or style.heading) then
        return s
    end
    local out = {}
    for _, ch in ipairs(chars(s)) do
        if ch == " " then
            out[#out + 1] = ch
        elseif style.heading then
            out[#out + 1] = ch .. "\b" .. ch .. "\b" .. ch
        elseif style.bold and style.under then
            out[#out + 1] = "_\b" .. ch .. "\b" .. ch
        elseif style.bold then
            out[#out + 1] = ch .. "\b" .. ch
        else
            out[#out + 1] = "_\b" .. ch
        end
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

local function latex_replace_commands(s)
    return (s:gsub("\\(%a+)", function(cmd)
        return latex[cmd] or ("\\" .. cmd)
    end))
end

local function latex_replace_scripts(s)
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

local function render_math(math)
    math = latex_replace_frac(math)
    math = latex_replace_sqrt(math)
    math = latex_replace_commands(math)
    math = latex_replace_scripts(math)
    return (math:gsub("[{}]", ""))
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
                text[#text + 1] = render_math(s:sub(i + #delim, a - 1))
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
        elseif ch == "<" then
            local url, e = s:match("^<(%a[%w+.-]*:[^%s<>]*)>()", i)
            if url == nil then
                url, e = s:match("^<([%w.+-]+@[%w.-]+)>()", i)
            end
            local tag_end = url == nil and (s:match("^<!%-%-.-%-%->()", i) or s:match("^<[!/]?%a[^<>]*>()", i))
                or nil
            if url ~= nil then
                text[#text + 1] = url
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
            local alt, e = s:match("^!%[([^%]]*)%]%b()()", i)
            if alt == nil then
                alt, e = s:match("^!%[([^%]]*)%]%b[]()", i)
            end
            if alt ~= nil then
                text[#text + 1] = "[" .. alt .. "]"
                i = e
            else
                text[#text + 1] = ch
                i = i + 1
            end
        elseif ch == "[" then
            local label, e = s:match("^%[([^%[%]]*)%]()", i)
            local url
            if label ~= nil then
                local target, e2 = s:match("^%(([^%s)]*)[^)]*%)()", e)
                if target ~= nil then
                    url = target
                    e = e2
                else
                    local ref_end = s:match("^%[[^%]]*%]()", e)
                    if ref_end ~= nil then
                        e = ref_end
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
        elseif ch == "*" or ch == "_" then
            local _, e = s:find("^" .. (ch == "*" and "%*+" or "_+"), i)
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
            text[#text + 1] = ch
            i = i + 1
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
            local use = (opener[3] >= 2 and closer[3] >= 2) and 2 or 1
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
    local bold, under = 0, 0
    local function current()
        return {
            bold = style.bold or bold > 0,
            under = style.under or under > 0,
            heading = style.heading,
        }
    end
    for _, t in ipairs(tokens) do
        if t[1] == "text" then
            out[#out + 1] = styled(t[2], current())
        elseif t[1] == "code" then
            out[#out + 1] = styled(t[2], { under = true, heading = style.heading })
        elseif t[1] == "link" then
            local s = current()
            s.under = true
            render_tokens(t[2], s, out)
            if t[3] ~= nil and t[3] ~= "" then
                local plain = {}
                render_tokens(t[2], {}, plain)
                if table.concat(plain) ~= t[3] then
                    out[#out + 1] = styled(" <" .. t[3] .. ">", current())
                end
            end
        else
            if t.closes then
                for _, use in ipairs(t.closes) do
                    if use == 2 then
                        bold = bold - 1
                    else
                        under = under - 1
                    end
                end
            end
            if t[3] > 0 then
                out[#out + 1] = styled(t[2]:rep(t[3]), current())
            end
            if t.opens then
                for _, use in ipairs(t.opens) do
                    if use == 2 then
                        bold = bold + 1
                    else
                        under = under + 1
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
-- style it (c, c\bc, _\bc, _\bc\bc), so that #units is the width on screen.
local function units_of(rendered)
    local cs = chars(rendered)
    local units = {}
    local i = 1
    while i <= #cs do
        local unit = cs[i]
        i = i + 1
        while cs[i] == "\b" and cs[i + 1] ~= nil do
            unit = unit .. "\b" .. cs[i + 1]
            i = i + 2
        end
        units[#units + 1] = unit
    end
    return units
end

-- The words of rendered text, packed into lines no wider than w units; a
-- space is never overstruck, so it is always a unit of its own.
local function wrap_units(units, w)
    local segs = {}
    local cur = {}
    local word = {}

    local function push_word()
        if #word == 0 then
            return
        end
        while #word > w do
            if #cur > 0 then
                segs[#segs + 1] = cur
                cur = {}
            end
            segs[#segs + 1] = { table.unpack(word, 1, w) }
            word = { table.unpack(word, w + 1) }
        end
        if #cur == 0 then
            cur = word
        elseif #cur + 1 + #word <= w then
            cur[#cur + 1] = " "
            for _, u in ipairs(word) do
                cur[#cur + 1] = u
            end
        else
            segs[#segs + 1] = cur
            cur = word
        end
        word = {}
    end

    for _, u in ipairs(units) do
        if u == " " then
            push_word()
        else
            word[#word + 1] = u
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
            if #units[r][c] > colw[c] then
                colw[c] = #units[r][c]
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
    for r = 1, #rows do
        cells[r] = {}
        for c = 1, maxc do
            local u = units[r][c]
            cells[r][c] = #u > colw[c] and wrap_units(u, colw[c]) or { u }
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
                local pad = colw[c] - #cell
                local left, right = 0, pad
                if align[c] == "right" then
                    left, right = pad, 0
                elseif align[c] == "center" then
                    left = pad // 2
                    right = pad - left
                end
                parts[c] = (" "):rep(left) .. table.concat(cell) .. (" "):rep(right)
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
    for line in (text .. "\n"):gmatch("(.-)\r?\n") do
        lines[#lines + 1] = line
    end
    if #lines > 0 and lines[#lines] == "" and text:sub(-1) == "\n" then
        lines[#lines] = nil
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
        local t = trim(line)
        if buf ~= nil then
            if t == delim then
                out[#out + 1] = delim .. table.concat(buf, " ") .. delim
                buf = nil
            else
                buf[#buf + 1] = t
            end
        elseif t == "$" or t == "$$" then
            buf = {}
            delim = t
        else
            out[#out + 1] = line
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

local function is_atx_heading(line)
    return line:match("^ ? ? ?#+$") ~= nil or line:match("^ ? ? ?#+%s") ~= nil
end

-- Whether the line opens a block of its own, which ends the paragraph
-- before it.
local function starts_block(line, next_line)
    return is_blank(line) or fence_of(line) ~= nil or is_hr(line) or is_atx_heading(line)
        or list_item(line) ~= nil or line:match("^%s*>") ~= nil
        or (line:find("|", 1, true) ~= nil and next_line ~= nil and is_table_sep(next_line))
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
    for _, s in ipairs(logical) do
        for rendered in (inline(s, {}) .. "\n"):gmatch("(.-)\n") do
            for _, seg in ipairs(wrap_units(units_of(rendered), room)) do
                out[#out + 1] = prefix .. table.concat(seg)
                prefix = hanging
            end
        end
    end
end

function M.render(text, opts)
    local width_limit = opts and opts.width or M.DEFAULT_WIDTH
    local lines = join_display_math(split_lines(text))
    local out = {}
    local i = 1
    local prev_blank = true
    local prev_list = false

    while i <= #lines do
        local line = lines[i]
        local next_line = lines[i + 1]
        local fence = fence_of(line)
        local blank = is_blank(line)
        local was_list = prev_list
        prev_list = blank and was_list

        if fence ~= nil then
            local fence_char = fence:sub(1, 1)
            i = i + 1
            while i <= #lines do
                local close = lines[i]:match("^ ? ? ?(" .. (fence_char == "`" and "```+" or "~~~+") .. ")%s*$")
                if close ~= nil and #close >= #fence then
                    break
                end
                out[#out + 1] = "    " .. lines[i]
                i = i + 1
            end
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
                out[#out + 1] = lines[i]
                i = i + 1
            end
        elseif line:find("|", 1, true) and next_line ~= nil and is_table_sep(next_line) then
            local rows = { line, next_line }
            i = i + 2
            while i <= #lines and lines[i]:find("|", 1, true) and not is_blank(lines[i]) do
                rows[#rows + 1] = lines[i]
                i = i + 1
            end
            render_table(rows, out, width_limit)
        elseif is_hr(line) then
            out[#out + 1] = BOX_H:rep(width_limit)
            i = i + 1
        elseif is_atx_heading(line) then
            local rest = line:match("^ ? ? ?#+%s*(.-)%s*$")
            rest = rest:gsub("%s+#+$", ""):gsub("^#+$", "")
            out[#out + 1] = inline(rest, { heading = true })
            i = i + 1
        elseif next_line ~= nil and not was_list and list_item(line) == nil
            and (next_line:match("^ ? ? ?=+%s*$") or next_line:match("^ ? ? ?%-+%s*$")) then
            out[#out + 1] = inline(trim(line), { heading = true })
            i = i + 2
        elseif line:match("^ ? ? ?%[[^%]]+%]:%s+%S") then
            i = i + 1
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
                if marker:match("^%d") then
                    prefix = prefix .. indent .. marker .. " "
                else
                    prefix = prefix .. indent .. BULLET .. " "
                end
                rest = item
                prev_list = true
            elseif was_list and rest:match("^%s") then
                prefix = prefix .. rest:match("^(%s*)")
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
    end
    return table.concat(out, "\n") .. "\n"
end

return M
