-- A paragraph, a list item or a quote: the lines are joined into one flow
-- and broken again at the width of the screen, behind the prefix the block
-- is drawn with.

local txt = require("text")
local spans = require("spans")
local inl = require("inline")

local width = txt.width

local units_of = spans.of_text
local sgr_line = spans.line
local wrap_units = spans.wrap

local inline = inl.render

local M = {}

-- The lines of a paragraph, list item or quote joined into one flow and
-- wrapped to the width: the first line behind prefix, the rest behind as
-- many spaces.  A line that ends in two spaces or a backslash keeps its
-- break, and so does a <br>.
local function flow(pieces, prefix, width_limit, out, doc)
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
        for rendered in (inline(s, {}, doc) .. "\n"):gmatch("(.-)\n") do
            for _, seg in ipairs(wrap_units(units_of(rendered), room)) do
                out[#out + 1] = prefix .. sgr_line(seg, sgr)
                prefix = hanging
            end
        end
    end
end

M.emit = flow

return M
