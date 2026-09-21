-- LaTeX in $...$: the symbols, \frac, \sqrt, the accents and the scripts,
-- turned into the Unicode a terminal can draw.  A formula that needs more
-- than one line is left to formula.env.

local txt = require("text")

local chars = txt.chars

local M = {}


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


M.render = render_math

return M
