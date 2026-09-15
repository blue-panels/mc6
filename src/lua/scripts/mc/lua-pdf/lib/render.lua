--[[
   PDF viewer lua-pdf for the M-Commander
   Page layout: PDF coordinates into terminal cells, and the byte stream

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

-- A page becomes one stream of rows, as tall as the page needs: the viewer
-- scrolls it the way it scrolls any terminal output, and the menu changes
-- the page.  Two ways to lay a page out.
--
-- "text": a line of the document is a row of the screen, and a character of
-- the document is about a cell wide.  A page then takes the rows it has
-- lines, not the rows its paper is tall, which is what makes it readable.
-- The pictures follow the same mapping, each keeping its proportions.
--
-- "page" and "fit": the paper as it is, scaled to the width of the viewer or
-- to the whole window.  Everything stays where it was printed.

local M = {}

local ESC = "\27"

local DEFAULT_CHAR_WIDTH = 7.0

local function clamp(value, low, high)
    if value < low then
        return low
    end
    if value > high then
        return high
    end
    return value
end

-- The columns the text takes on the screen.  ASCII is a column a byte; for
-- the rest the terminal is asked, because a character of the far east takes
-- two columns and an accent none.
local function text_columns(text)
    if not text:find("[\128-\255]") then
        return #text
    end
    if mc ~= nil and mc.ui ~= nil and mc.ui.text_width ~= nil then
        local width = mc.ui.text_width(text)

        if width ~= nil then
            return width
        end
    end
    local ok, len = pcall(utf8.len, text)

    if ok and len ~= nil then
        return len
    end
    return #text
end

-- The head of the text that fits in @limit columns, whole characters only.
local function clip(text, limit)
    if limit <= 0 then
        return ""
    end
    if text_columns(text) <= limit then
        return text
    end

    local ok, count = pcall(utf8.len, text)
    local take = (ok and count ~= nil) and math.min(count, limit) or nil

    if take == nil then
        return text:sub(1, limit)
    end
    -- Every character is at least one column, so @limit of them is the most
    -- that can fit; the wide ones are given back one at a time.
    while take > 0 do
        local offset = utf8.offset(text, take + 1)
        local head = text:sub(1, (offset or (#text + 1)) - 1)

        if text_columns(head) <= limit then
            return head
        end
        take = take - 1
    end
    return ""
end

M.clip = clip
M.text_columns = text_columns

local function median(values, fallback)
    if #values == 0 then
        return fallback
    end
    table.sort(values)
    return values[(#values + 1) // 2]
end

------------------------------------------------------------------------
-- The lines of the page.

-- Runs that share a baseline are one line.  The lines come back in reading
-- order, each with the row it gets: consecutive rows, and a blank row where
-- the paper has a gap of more than one line.
local function lines_of(page)
    local runs = {}
    local heights = {}

    for index, run in ipairs(page.texts) do
        runs[#runs + 1] = { index = index, y = run.top + run.height / 2, run = run }
        if run.height > 0 then
            heights[#heights + 1] = run.height
        end
    end
    table.sort(runs, function(a, b)
        if a.y == b.y then
            return a.run.left < b.run.left
        end
        return a.y < b.y
    end)

    local height = median(heights, 12)
    local tolerance = height * 0.6
    local lines = {}
    local line_of_run = {}

    for _, item in ipairs(runs) do
        local line = lines[#lines]
        if line == nil or item.y - line.y > tolerance then
            line = { y = item.y, count = 0, sum = 0 }
            lines[#lines + 1] = line
        end
        line.count = line.count + 1
        line.sum = line.sum + item.y
        line.y = line.sum / line.count
        line_of_run[item.index] = #lines
    end

    local gaps = {}
    for index = 2, #lines do
        gaps[#gaps + 1] = lines[index].y - lines[index - 1].y
    end
    local spacing = median(gaps, height * 1.25)
    if spacing <= 0 then
        spacing = height * 1.25
    end

    local row = 0
    for index, line in ipairs(lines) do
        if index > 1 then
            local blanks = clamp(math.floor((line.y - lines[index - 1].y) / spacing + 0.5) - 1, 0, 2)
            row = row + 1 + blanks
        end
        line.row = row
    end

    return { lines = lines, line_of_run = line_of_run, spacing = spacing, height = height }
end

-- The row a coordinate of the page falls on.  Between two lines of text the
-- distance is measured in rows of the page itself (@map.uy units each), so a
-- picture in a gap gets the rows it is tall; it never reaches past the line
-- below it, which is what reserve_rows() makes room for.
local function row_at(map, y)
    local lines = map.lines
    local count = #lines
    local uy = map.uy

    if count == 0 then
        return y / uy
    end
    if y <= lines[1].y then
        return lines[1].row - (lines[1].y - y) / uy
    end
    for index = 2, count do
        if y <= lines[index].y then
            return math.min(lines[index - 1].row + (y - lines[index - 1].y) / uy,
                            lines[index].row)
        end
    end
    return lines[count].row + (y - lines[count].y) / uy
end

-- The width of one character of the document, and the box its content sits
-- in: the margins of the paper are not worth a column each.
local function content_box(page)
    local widths = {}
    local left, right = nil, nil

    local function extend(a, b)
        left = left == nil and a or math.min(left, a)
        right = right == nil and b or math.max(right, b)
    end

    for _, run in ipairs(page.texts) do
        local columns = text_columns(run.text)
        if run.width > 0 and columns > 0 then
            widths[#widths + 1] = run.width / columns
        end
        extend(run.left, run.left + run.width)
    end
    for _, image in ipairs(page.images) do
        extend(image.left, image.left + image.width)
    end
    if left == nil then
        left, right = 0, page.width
    end
    return median(widths, DEFAULT_CHAR_WIDTH), left, math.max(right, left + 1)
end

------------------------------------------------------------------------
-- The plan of one page.

local function block(plan, row_from, row_to, col_from, col_to)
    for row = row_from, row_to do
        local list = plan.blocked[row]
        if list == nil then
            list = {}
            plan.blocked[row] = list
        end
        list[#list + 1] = { from = col_from, to = col_to }
    end
end

-- How many columns are free from @col on; 0 when @col is under a picture.
local function room(plan, row, col, columns)
    local limit = columns - col + 1
    local list = plan.blocked[row]

    if list ~= nil then
        for _, span in ipairs(list) do
            if col >= span.from and col <= span.to then
                return 0
            end
            if span.from > col and span.from - col < limit then
                limit = span.from - col
            end
        end
    end
    return limit
end

-- The cells one picture covers: the same scale across and down, so it keeps
-- the proportions it has on the paper.  A picture larger than the window is
-- drawn smaller: the viewer draws a picture whole or not at all.
local function image_cells(image, view, ux, uy)
    local cols = math.max(1, math.floor(image.width / ux + 0.5))
    local rows = math.max(1, math.floor(image.height / uy + 0.5))
    local max_rows = math.max(1, view.lines - 1)

    if cols > view.columns then
        rows = math.max(1, math.floor(rows * view.columns / cols))
        cols = view.columns
    end
    if rows > max_rows then
        cols = math.max(1, math.floor(cols * max_rows / rows))
        rows = max_rows
    end
    return cols, rows
end

-- A picture sits between two lines of text, and those two lines are next to
-- each other: the rows it needs are made by pushing the text below it down.
local function reserve_rows(map, page, view, ux, uy)
    local images = {}

    for _, image in ipairs(page.images) do
        images[#images + 1] = image
    end
    table.sort(images, function(a, b)
        return a.top < b.top
    end)

    for _, image in ipairs(images) do
        local bottom = image.top + image.height
        local _, rows = image_cells(image, view, ux, uy)
        local have = math.floor(row_at(map, bottom) - row_at(map, image.top) + 0.5)
        local delta = rows - have

        if delta > 0 then
            for _, line in ipairs(map.lines) do
                if line.y >= bottom then
                    line.row = line.row + delta
                end
            end
        end
    end
end

-- Rows are the rows of the page, from 1; the viewer scrolls them.
function M.plan(page, view, params)
    local columns = view.columns
    local aspect = view.cell_h / view.cell_w
    local zoom = params.zoom or 1.0
    local mode = params.mode or "text"

    -- A page with no text layer is a picture of a page: the rows of text it
    -- has none of cannot be what its height is measured in.
    if mode == "text" and #page.texts == 0 then
        mode = "fit"
    end

    local map = lines_of(page)
    local char_width, left, right = content_box(page)
    local ux, uy, x0, row_of

    if mode == "text" then
        ux = math.max(char_width, (right - left) / columns) / zoom
        uy = ux * aspect
        x0 = left
        map.uy = uy
        if map.spacing <= 0 or #map.lines == 0 then
            map.spacing = uy
        end
        reserve_rows(map, page, view, ux, uy)
        row_of = function(y)
            return row_at(map, y)
        end
    else
        ux = page.width / (columns * zoom)
        if mode == "fit" and page.height > 0 then
            local area_rows = math.max(1, view.lines - 1)
            ux = math.max(ux, page.height / (area_rows * aspect * zoom))
        end
        uy = ux * aspect
        x0 = 0
        row_of = function(y)
            return y / uy
        end
    end
    if ux <= 0 then
        ux = DEFAULT_CHAR_WIDTH
        uy = ux * aspect
    end

    local hit = params.hit
    local plan = {
        mode = mode,
        ux = ux,
        uy = uy,
        gap = char_width * 0.35,
        rows = {},
        images = {},
        blocked = {},
        total_rows = 1,
        hit_row = nil,
    }

    if hit ~= nil and hit.page ~= params.page then
        hit = nil
    end

    local function keep_height(row)
        if row > plan.total_rows then
            plan.total_rows = row
        end
    end

    keep_height(math.ceil(row_of(page.height)))

    for _, image in ipairs(page.images) do
        local cols, rows = image_cells(image, view, ux, uy)
        local col = clamp(math.floor((image.left - x0) / ux) + 1, 1, math.max(1, columns - cols + 1))
        local row = math.max(1, math.floor(row_of(image.top) + 0.5) + 1)

        plan.images[#plan.images + 1] = {
            src = image.src,
            row = row,
            col = col,
            cols = cols,
            rows = rows,
            width = cols * view.cell_w,
            height = rows * view.cell_h,
        }
        block(plan, row, row + rows - 1, col, col + cols - 1)
        keep_height(row + rows - 1)
    end

    for index, run in ipairs(page.texts) do
        local row
        if mode == "text" then
            local line = map.lines[map.line_of_run[index]]
            row = line ~= nil and line.row or math.floor(row_of(run.top + run.height / 2))
        else
            row = math.floor(row_of(run.top + run.height / 2))
        end
        row = row + 1
        if row >= 1 then
            local list = plan.rows[row]
            if list == nil then
                list = {}
                plan.rows[row] = list
            end
            local entry = {
                col = math.max(1, math.floor((run.left - x0) / ux) + 1),
                left = run.left,
                right = run.left + run.width,
                text = run.text,
                bold = run.bold,
            }

            if hit ~= nil and hit.run == index then
                entry.mark_from = hit.from
                entry.mark_to = hit.to
                plan.hit_row = row
            end
            list[#list + 1] = entry
            keep_height(row)
        end
    end
    for _, list in pairs(plan.rows) do
        table.sort(list, function(a, b)
            if a.col == b.col then
                return a.left < b.left
            end
            return a.col < b.col
        end)
    end

    return plan
end

------------------------------------------------------------------------
-- The bytes of the page.

-- The stream is written the way a program writes to a terminal: down with
-- line feeds, back up with the cursor, never to a row by its number.  The
-- rows that leave the top are the scrollback the viewer pages through.
local function move(out, state, row, col)
    if row > state.row then
        out[#out + 1] = string.rep("\n", row - state.row)
        state.col = 1
    elseif row < state.row then
        out[#out + 1] = string.format("%s[%dA", ESC, state.row - row)
    end
    state.row = row
    if col ~= state.col then
        out[#out + 1] = string.format("%s[%dG", ESC, col)
        state.col = col
    end
end

-- @picture returns the bytes of a picture and how they are drawn: "sixel",
-- one block the terminal puts at the cursor, or "symbols", the rows of
-- characters chafa draws it with.  Where it returns nothing a label goes in
-- the place of the picture.  The first row of the page is the first row of
-- the output: what page it is the viewer says in its status line, not the
-- page itself.
function M.compose(plan, view, picture)
    local out = { ESC .. "[?7l", ESC .. "[2J", ESC .. "[H" }
    local state = { row = 1, col = 1 }
    local items = {}

    for row in pairs(plan.rows) do
        items[#items + 1] = { row = row, text = true }
    end
    for _, image in ipairs(plan.images) do
        items[#items + 1] = { row = image.row, image = image }
    end
    table.sort(items, function(a, b)
        if a.row == b.row then
            return (a.text and 0 or 1) < (b.text and 0 or 1)
        end
        return a.row < b.row
    end)

    for _, item in ipairs(items) do
        if item.text then
            local cursor = 1
            local printed = nil
            for _, run in ipairs(plan.rows[item.row]) do
                local col = math.max(run.col, cursor)
                -- Runs the paper keeps apart do not grow together here.
                if printed ~= nil and col <= cursor and run.left > printed + plan.gap then
                    col = cursor + 1
                end
                local text = clip(run.text, room(plan, item.row, col, view.columns))
                if text ~= "" then
                    move(out, state, item.row, col)
                    if run.bold then
                        out[#out + 1] = ESC .. "[1m"
                    end
                    if run.mark_from ~= nil and run.mark_from <= #text then
                        -- what the search found, in reverse video
                        local last = math.min(run.mark_to, #text)

                        out[#out + 1] = text:sub(1, run.mark_from - 1)
                        out[#out + 1] = ESC .. "[7m"
                        out[#out + 1] = text:sub(run.mark_from, last)
                        out[#out + 1] = ESC .. "[27m"
                        out[#out + 1] = text:sub(last + 1)
                    else
                        out[#out + 1] = text
                    end
                    if run.bold then
                        out[#out + 1] = ESC .. "[m"
                    end
                    state.col = col + text_columns(text)
                    cursor = state.col
                    printed = run.right
                end
            end
        else
            local image = item.image
            local data, kind = picture(image)

            if data == nil then
                local text = clip("[image]", view.columns - image.col + 1)

                move(out, state, image.row, image.col)
                out[#out + 1] = text
                state.col = image.col + text_columns(text)
            elseif kind == "symbols" then
                -- A row of characters per row of the picture, each put in the
                -- column the picture starts at.
                local row = image.row

                for line in (data .. "\n"):gmatch("(.-)\n") do
                    if row > image.row + image.rows - 1 or row > view.lines then
                        break
                    end
                    if line ~= "" then
                        move(out, state, row, image.col)
                        out[#out + 1] = line
                        out[#out + 1] = ESC .. "[m"
                        state.col = image.col + image.cols
                    end
                    row = row + 1
                end
            else
                move(out, state, image.row, image.col)
                out[#out + 1] = data
                -- The picture leaves the cursor on the row below it.
                state.row = image.row + image.rows
                state.col = image.col
            end
        end
    end

    move(out, state, plan.total_rows, 1)
    return table.concat(out)
end

return M
