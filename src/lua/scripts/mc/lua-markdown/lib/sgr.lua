-- What the terminal is told: the escape sequences of a style, the
-- overstrikes the viewer paints bold and underline with, and the colors a
-- code block is given.  Nothing here knows about markdown.

local cfg = require("config")
local txt = require("text")

local chars = txt.chars

local M = {}

-- the combining long stroke that strikes a character through; the terminal
-- has no attribute for it
local STRIKE = "\u{0336}"

local SGR_ITALIC = "\27[3m"
local SGR_ITALIC_OFF = "\27[23m"
local SGR_COLOR_OFF = "\27[39m"
local SGR_BG_OFF = "\27[49m"
-- closes what a rule of the syntax engine opened; a plain reset would close
-- the background of the code block too
local SGR_RUN_OFF = "\27[22;23;24;27;39m"
local LINK_END = "\27]8;;\27\\"

-- The OSC 8 sequence that starts a link to url; bytes that could end the
-- sequence early are dropped and spaces are escaped.
local function link_start(url)
    return "\27]8;;" .. url:gsub("[%c]", ""):gsub(" ", "%%20") .. "\27\\"
end


-- style: { bold = b, under = u, italic = i, strike = s, heading = level }.  A space is
-- never overstruck, and the SGR sequences go around the visible characters

-- only, so that a line never starts or ends inside them with a space.
local function styled(s, style)
    local open, close = "", ""
    if style.italic then
        open, close = SGR_ITALIC, SGR_ITALIC_OFF
    end
    local color = style.heading and cfg.HEADING_COLORS[style.heading]
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
-- The colors of the rules of the editor, and of a code block.

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

-- The colors a skin names, as the terminal draws them.  A name it does not
-- know, and the "default" of the terminal itself, are taken for a dark
-- background: that is what a terminal running mc almost always has.
local SKIN_RGB = {
    black = { 0, 0, 0 }, red = { 128, 0, 0 }, green = { 0, 128, 0 },
    brown = { 128, 128, 0 }, blue = { 0, 0, 128 }, magenta = { 128, 0, 128 },
    cyan = { 0, 128, 128 }, lightgray = { 192, 192, 192 }, gray = { 128, 128, 128 },
    brightred = { 255, 0, 0 }, brightgreen = { 0, 255, 0 }, yellow = { 255, 255, 0 },
    brightblue = { 0, 0, 255 }, brightmagenta = { 255, 0, 255 },
    brightcyan = { 0, 255, 255 }, white = { 255, 255, 255 },
}

-- The steps of the 6x6x6 cube of a terminal of 256 colors.
local CUBE_STEPS = { 0, 95, 135, 175, 215, 255 }

-- What the skin means by a color name, as red, green and blue.
local function skin_rgb(name)
    if name == nil or name == "" or name == "default" then
        return { 0, 0, 0 }
    end
    local hex = name:match("^#(%x%x%x%x%x%x)$")

    if hex ~= nil then
        return {
            tonumber(hex:sub(1, 2), 16),
            tonumber(hex:sub(3, 4), 16),
            tonumber(hex:sub(5, 6), 16),
        }
    end
    local index = tonumber(name:match("^color(%d+)$") or "")

    if index ~= nil and index >= 232 and index <= 255 then
        local gray = 8 + (index - 232) * 10

        return { gray, gray, gray }
    end
    if index ~= nil and index >= 16 and index <= 231 then
        local n = index - 16

        return {
            CUBE_STEPS[math.floor(n / 36) % 6 + 1],
            CUBE_STEPS[math.floor(n / 6) % 6 + 1],
            CUBE_STEPS[n % 6 + 1],
        }
    end
    return SKIN_RGB[name] or { 0, 0, 0 }
end

-- The nearest color of a terminal of 256: the gray ramp for a gray, the cube
-- for everything else.
local function rgb_to_256(r, g, b)
    if math.abs(r - g) < 10 and math.abs(g - b) < 10 then
        local gray = math.floor((r + g + b) / 3)

        if gray < 8 then
            return 16
        end
        if gray > 238 then
            return 231
        end
        return 232 + math.floor((gray - 8) / 10)
    end
    local function step(v)
        local best, best_d = 0, 1e9

        for i, s in ipairs(CUBE_STEPS) do
            local d = math.abs(s - v)

            if d < best_d then
                best, best_d = i - 1, d
            end
        end
        return best
    end

    return 16 + 36 * step(r) + 6 * step(g) + step(b)
end

-- The SGR background of a code block: a shade of the one the skin paints the
-- viewer with, dark backgrounds lightened and light ones darkened.  A
-- terminal of fewer than 256 colors keeps its background: the sixteen it has
-- are too far apart for a shade.
local function auto_bg()
    if mc == nil or mc.tty == nil or mc.tty.info == nil then
        return nil
    end
    local info = mc.tty.info("Viewer")

    if info == nil or info.colors == nil or info.colors < 256 then
        return nil
    end
    local rgb = skin_rgb(info.bg)
    local shift = (rgb[1] + rgb[2] + rgb[3]) / 3 < 128 and cfg.CODE_BG_SHIFT or -cfg.CODE_BG_SHIFT
    local out = {}

    for i = 1, 3 do
        out[i] = math.max(0, math.min(255, rgb[i] + shift))
    end
    if info.colors >= 1 << 24 then
        return "48;2;" .. out[1] .. ";" .. out[2] .. ";" .. out[3]
    end
    return "48;5;" .. rgb_to_256(out[1], out[2], out[3])
end

-- The SGR background in force, nil when the block is drawn without one.
local function code_bg()
    if cfg.CODE_BG == "auto" then
        return auto_bg()
    end
    if cfg.CODE_BG == nil or cfg.CODE_BG == "" then
        return nil
    end
    return cfg.CODE_BG
end

M.STRIKE = STRIKE
M.SGR_ITALIC = SGR_ITALIC
M.SGR_ITALIC_OFF = SGR_ITALIC_OFF
M.SGR_COLOR_OFF = SGR_COLOR_OFF
M.SGR_BG_OFF = SGR_BG_OFF
M.SGR_RUN_OFF = SGR_RUN_OFF
M.LINK_END = LINK_END
M.link_start = link_start
M.styled = styled
M.of_color = sgr_of_color
M.code_bg = code_bg

return M
