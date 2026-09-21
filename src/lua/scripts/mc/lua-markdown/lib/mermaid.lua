-- Mermaid diagrams as text.  What the parsers do not know is left to the
-- caller, which shows the block as code.

local txt = require("text")
local flowchart = require("mermaid.flowchart")
local class = require("mermaid.class")
local pie = require("mermaid.pie")
local sequence = require("mermaid.sequence")

local trim = txt.trim

local M = {}

function M.render(code, width_limit)
    local lines = {}

    for line in (code .. "\n"):gmatch("(.-)\n") do
        lines[#lines + 1] = line
    end

    local first = nil
    for _, line in ipairs(lines) do
        if trim(line) ~= "" then
            first = trim(line)
            break
        end
    end
    if first == nil then
        return nil
    end

    if first:match("^pie") then
        local chart = pie.parse(lines)

        return chart ~= nil and pie.draw(chart, width_limit) or nil
    end
    if first:match("^sequenceDiagram") then
        local seq = sequence.parse(lines)

        return seq ~= nil and sequence.draw(seq) or nil
    end
    if first:match("^classDiagram") then
        local model = class.parse(lines)

        return model ~= nil and class.draw(model, width_limit) or nil
    end
    if first:match("^graph%s") or first:match("^flowchart%s") then
        local chart = flowchart.parse(lines)

        if chart ~= nil then
            -- boxes when they fit on the screen, a tree when they do not
            local boxes = flowchart.draw_boxes(chart, width_limit)

            if boxes ~= nil then
                return boxes
            end
        end

        return chart ~= nil and flowchart.draw_tree(chart) or nil
    end
    return nil
end

return M
