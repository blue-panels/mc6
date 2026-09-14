--[[
   PDF viewer lua-pdf for the M-Commander
   Reader for the XML that pdftohtml writes

   Copyright (C) 2026
   Ilia Maslakov il.smind@gmail.com

   This file is part of M-Commander.

   M-Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   M-Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see https://www.gnu.org/licenses/.
]]

-- pdftohtml -xml gives one <page> per page, a <text> per run of characters
-- and an <image> per picture it wrote out, all with absolute coordinates in
-- the same units: the page at its -r resolution, 150 dpi by default.

local M = {}

local ENTITIES = { amp = "&", lt = "<", gt = ">", quot = "\"", apos = "'" }

local function decode(text)
    if text:find("&", 1, true) == nil then
        return text
    end
    return (text:gsub("&(#?[%w]+);", function(name)
        local head = name:sub(1, 1)
        if head == "#" then
            local code
            local second = name:sub(2, 2)
            if second == "x" or second == "X" then
                code = tonumber(name:sub(3), 16)
            else
                code = tonumber(name:sub(2))
            end
            if code == nil or code < 0 or code > 0x10FFFF then
                return nil
            end
            return utf8.char(code)
        end
        return ENTITIES[name]
    end))
end

local function attributes(text)
    local attrs = {}
    for name, value in text:gmatch('([%w_:%-]+)%s*=%s*"([^"]*)"') do
        attrs[name] = value
    end
    return attrs
end

local function number(value, fallback)
    return tonumber(value) or fallback
end

-- The tags a run of text carries: <b>, <i>, and the <a href> of a link.
local function run_style(body)
    return body:find("<b>", 1, true) ~= nil, body:find("<i>", 1, true) ~= nil
end

-- Pages in file order, each with its box and its two lists.  A page with no
-- text layer comes back with an empty texts list: that is a scan.
function M.parse(text)
    local pages = {}
    local page = nil
    local pos = 1

    while true do
        local start, stop, name, rest = text:find("<([%w_]+)([^>]*)>", pos)
        if start == nil then
            break
        end
        pos = stop + 1

        if name == "page" then
            local attrs = attributes(rest)
            page = {
                number = math.tointeger(number(attrs.number, #pages + 1)) or (#pages + 1),
                width = number(attrs.width, 0),
                height = number(attrs.height, 0),
                texts = {},
                images = {},
            }
            pages[#pages + 1] = page
        elseif name == "image" and page ~= nil then
            local attrs = attributes(rest)
            if attrs.src ~= nil then
                page.images[#page.images + 1] = {
                    left = number(attrs.left, 0),
                    top = number(attrs.top, 0),
                    width = number(attrs.width, 0),
                    height = number(attrs.height, 0),
                    src = decode(attrs.src),
                }
            end
        elseif name == "text" and page ~= nil then
            local attrs = attributes(rest)
            local body = ""
            local close_start, close_stop = text:find("</text>", pos, true)
            if close_start ~= nil then
                body = text:sub(pos, close_start - 1)
                pos = close_stop + 1
            end
            local bold, italic = run_style(body)
            local plain = decode((body:gsub("<[^>]*>", "")))
            if plain ~= "" then
                page.texts[#page.texts + 1] = {
                    left = number(attrs.left, 0),
                    top = number(attrs.top, 0),
                    width = number(attrs.width, 0),
                    height = number(attrs.height, 0),
                    text = plain,
                    bold = bold,
                    italic = italic,
                }
            end
        end
    end

    return pages
end

return M
