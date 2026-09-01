local active_style = nil
local indicator_id = "box-drawing"

local LEFT, RIGHT, UP, DOWN = 1, 2, 3, 4

local glyph_for, connections = {}, {}

local function state_key(state)
    return table.concat(state, ",")
end

local function define(glyph, left, right, up, down)
    local state = { left, right, up, down }
    glyph_for[state_key(state)] = glyph
    connections[glyph] = state
end

define(" ", 0, 0, 0, 0)

-- Single lines.
define("╴", 1, 0, 0, 0); define("╶", 0, 1, 0, 0)
define("╵", 0, 0, 1, 0); define("╷", 0, 0, 0, 1)
define("─", 1, 1, 0, 0); define("│", 0, 0, 1, 1)
define("┌", 0, 1, 0, 1); define("┐", 1, 0, 0, 1)
define("└", 0, 1, 1, 0); define("┘", 1, 0, 1, 0)
define("├", 0, 1, 1, 1); define("┤", 1, 0, 1, 1)
define("┬", 1, 1, 0, 1); define("┴", 1, 1, 1, 0)
define("┼", 1, 1, 1, 1)

-- Double lines. Heavy half-lines are used only while an endpoint is open.
define("╸", 2, 0, 0, 0); define("╺", 0, 2, 0, 0)
define("╹", 0, 0, 2, 0); define("╻", 0, 0, 0, 2)
define("═", 2, 2, 0, 0); define("║", 0, 0, 2, 2)
define("╔", 0, 2, 0, 2); define("╗", 2, 0, 0, 2)
define("╚", 0, 2, 2, 0); define("╝", 2, 0, 2, 0)
define("╠", 0, 2, 2, 2); define("╣", 2, 0, 2, 2)
define("╦", 2, 2, 0, 2); define("╩", 2, 2, 2, 0)
define("╬", 2, 2, 2, 2)

-- Mixed single/double junctions from the Unicode box-drawing block.
define("╒", 0, 2, 0, 1); define("╓", 0, 1, 0, 2)
define("╕", 2, 0, 0, 1); define("╖", 1, 0, 0, 2)
define("╘", 0, 2, 1, 0); define("╙", 0, 1, 2, 0)
define("╛", 2, 0, 1, 0); define("╜", 1, 0, 2, 0)
define("╞", 0, 2, 1, 1); define("╟", 0, 1, 2, 2)
define("╡", 2, 0, 1, 1); define("╢", 1, 0, 2, 2)
define("╤", 2, 2, 0, 1); define("╥", 1, 1, 0, 2)
define("╧", 2, 2, 1, 0); define("╨", 1, 1, 2, 0)
define("╪", 2, 2, 1, 1); define("╫", 1, 1, 2, 2)

local function render(state)
    local glyph = glyph_for[state_key(state)]
    if glyph ~= nil then return glyph end

    -- Unicode has no single/double glyph for different weights on opposite
    -- halves of one axis. Promote that axis to the stronger weight.
    local horizontal = math.max(state[LEFT], state[RIGHT])
    local vertical = math.max(state[UP], state[DOWN])
    local normalized = {
        state[LEFT] > 0 and horizontal or 0,
        state[RIGHT] > 0 and horizontal or 0,
        state[UP] > 0 and vertical or 0,
        state[DOWN] > 0 and vertical or 0,
    }
    return glyph_for[state_key(normalized)] or "┼"
end

local opposite_side = { [LEFT] = RIGHT, [RIGHT] = LEFT, [UP] = DOWN, [DOWN] = UP }

local directions = {
    left = { line = 0, column = -1, here = LEFT, there = RIGHT },
    right = { line = 0, column = 1, here = RIGHT, there = LEFT },
    up = { line = -1, column = 0, here = UP, there = DOWN },
    down = { line = 1, column = 0, here = DOWN, there = UP },
}

local function split_document(text)
    local lines, starts = {}, {}
    local start = 1

    while true do
        starts[#starts + 1] = start - 1
        local newline = text:find("\n", start, true)
        if newline == nil then
            lines[#lines + 1] = text:sub(start)
            break
        end
        lines[#lines + 1] = text:sub(start, newline - 1)
        start = newline + 1
    end
    return lines, starts
end

local function characters(text, tab_width, through_column)
    local result = {}
    local visual_column = 1
    local failure
    local ok = pcall(function()
        for _, codepoint in utf8.codes(text) do
            local character = utf8.char(codepoint)
            if character == "\t" and visual_column <= through_column then
                local spaces = tab_width - ((visual_column - 1) % tab_width)
                for _ = 1, spaces do result[#result + 1] = " " end
                visual_column = visual_column + spaces
            else
                local character_width, width_error = mc.ui.text_width(character)
                if character_width == nil then
                    failure = width_error
                    error(width_error)
                elseif character_width == 0 and #result > 0 then
                    local index = #result
                    while index > 1 and result[index] == "" do index = index - 1 end
                    result[index] = result[index] .. character
                else
                    result[#result + 1] = character
                    for _ = 2, math.max(character_width, 1) do result[#result + 1] = "" end
                    visual_column = visual_column + character_width
                end
            end
        end
    end)
    return ok and result or nil, failure
end

local function ensure_column(line, column)
    while #line < column do line[#line + 1] = " " end
end

local function add_connection(line, column, connection, style)
    ensure_column(line, column)
    if line[column] == "" then return false end
    local previous = connections[line[column]] or connections[" "]
    local state = { previous[1], previous[2], previous[3], previous[4] }
    local weight = style == "double" and 2 or 1

    state[connection] = weight
    -- The axis being drawn takes the active weight, so a single line redraws
    -- a double one instead of being promoted back to it.
    if state[opposite_side[connection]] ~= 0 then state[opposite_side[connection]] = weight end
    line[column] = render(state)
    return true
end

local function connection_of(character, side)
    local state = character ~= nil and connections[character] or nil
    return state ~= nil and state[side] or 0
end

local function is_frame(character)
    return character ~= nil and character ~= " " and connections[character] ~= nil
end

local function has_frame(text)
    local found = false
    pcall(function()
        for _, codepoint in utf8.codes(text) do
            if is_frame(utf8.char(codepoint)) then
                found = true
                return
            end
        end
    end)
    return found
end

-- A gap opened inside a horizontal line must keep the line, not break it.
local function filler_for(cells, column)
    local index = column
    while index > 1 and cells[index] == "" do index = index - 1 end

    local weight = math.max(connection_of(cells[index], LEFT),
        index > 1 and connection_of(cells[index - 1], RIGHT) or 0)
    if weight == 2 then return index, "═" end
    if weight == 1 then return index, "─" end
    return index, " "
end

-- Space on a frame character widens the whole table: every line of the table
-- is pushed right at that column, so the columns stay aligned.
local function expand(editor)
    if active_style == nil then return mc.PASS end

    -- In overwrite mode a space replaces the character under the cursor, which
    -- is how a frame is rubbed out, so the table widens only while Insert is
    -- on. A host that cannot report the typing mode always widens.
    if editor.overwrite ~= nil and editor:overwrite() then return mc.PASS end

    local line, column = editor:cursor()
    if line == nil then return mc.PASS end

    local tab_width = editor:tab_width()
    if tab_width == nil then return mc.PASS end

    local info = editor:info()
    if info == nil or info.byte_length == 0 then return mc.PASS end

    local text = editor:text { from = 0, to = info.byte_length, revision = info.revision }
    if text == nil then return mc.PASS end

    local source_lines, starts = split_document(text)
    if line > #source_lines then return mc.PASS end

    -- The cursor may sit on the frame or just before it.
    local cells = characters(source_lines[line], tab_width, column + 1)
    if cells == nil then return mc.PASS end
    if not is_frame(cells[column]) and not is_frame(cells[column + 1]) then return mc.PASS end

    local first, last = line, line
    while first > 1 and has_frame(source_lines[first - 1]) do first = first - 1 end
    while last < #source_lines and has_frame(source_lines[last + 1]) do last = last + 1 end

    local replacement_lines = {}
    for index = first, last do
        local row = characters(source_lines[index], tab_width, column)
        local widened = source_lines[index]

        if row ~= nil and #row >= column then
            local at, glyph = filler_for(row, column)
            table.insert(row, at, glyph)
            widened = table.concat(row)
        end
        replacement_lines[#replacement_lines + 1] = widened
    end

    local edit_result, replace_error = editor:replace({
        from = starts[first],
        to = starts[last] + #source_lines[last],
        revision = info.revision,
    }, table.concat(replacement_lines, "\n"))
    if edit_result == nil then
        mc.ui.status("Box drawing: " .. replace_error)
        return mc.CONSUME
    end

    local ok, cursor_error = editor:set_cursor(line, column + 1)
    if not ok then mc.ui.status("Box drawing: " .. cursor_error) end
    return mc.CONSUME
end

local function vertical_glyph(weight)
    if weight == 2 then return "║" end
    if weight == 1 then return "│" end
    return " "
end

local function last_frame_column(cells)
    local last = 0
    for index = 1, #cells do
        if is_frame(cells[index]) then last = index end
    end
    return last
end

-- The new row carries the vertical lines of the row it is cut from, so the
-- gap it opens keeps the table closed.
local function frame_row(cells, side)
    local row = {}
    local found = false

    for index = 1, #cells do
        row[index] = vertical_glyph(connection_of(cells[index], side))
        if row[index] ~= " " then found = true end
    end
    if not found then return nil end

    while #row > 0 and row[#row] == " " do table.remove(row) end
    return table.concat(row), #row
end

-- Enter past the right border of a table adds one more row to it.
local function grow(editor)
    if active_style == nil then return mc.PASS end

    local line, column = editor:cursor()
    if line == nil then return mc.PASS end

    local tab_width = editor:tab_width()
    if tab_width == nil then return mc.PASS end

    local info = editor:info()
    if info == nil or info.byte_length == 0 then return mc.PASS end

    local text = editor:text { from = 0, to = info.byte_length, revision = info.revision }
    if text == nil then return mc.PASS end

    local source_lines, starts = split_document(text)
    if line > #source_lines then return mc.PASS end

    local cells = characters(source_lines[line], tab_width, math.huge)
    if cells == nil or column <= last_frame_column(cells) then return mc.PASS end

    -- A bottom border has nothing below it, so the row goes above it instead.
    local target = line + 1
    local row, width = frame_row(cells, DOWN)
    if row == nil then
        target = line
        row, width = frame_row(cells, UP)
    end
    if row == nil then return mc.PASS end

    local edit_result, replace_error = editor:replace({
        from = starts[line],
        to = starts[line] + #source_lines[line],
        revision = info.revision,
    }, target > line and source_lines[line] .. "\n" .. row or row .. "\n" .. source_lines[line])
    if edit_result == nil then
        mc.ui.status("Box drawing: " .. replace_error)
        return mc.CONSUME
    end

    local ok, cursor_error = editor:set_cursor(target, math.min(column, width + 1))
    if not ok then mc.ui.status("Box drawing: " .. cursor_error) end
    return mc.CONSUME
end

local function draw(editor, direction_name)
    if active_style == nil then return mc.PASS end

    local direction = directions[direction_name]
    local style = active_style
    local line, column = editor:cursor()
    if line == nil then
        mc.ui.status("Box drawing: cannot read cursor")
        return mc.CONSUME
    end
    local tab_width, tab_error = editor:tab_width()
    if tab_width == nil then
        mc.ui.status("Box drawing: " .. tab_error)
        return mc.CONSUME
    end

    local target_line = line + direction.line
    local target_column = column + direction.column
    if target_line < 1 or target_column < 1 then return mc.CONSUME end

    local info, info_error = editor:info()
    if info == nil then
        mc.ui.status("Box drawing: " .. info_error)
        return mc.CONSUME
    end

    local text = ""
    if info.byte_length > 0 then
        text, info_error = editor:text {
            from = 0, to = info.byte_length, revision = info.revision,
        }
        if text == nil then
            mc.ui.status("Box drawing: " .. info_error)
            return mc.CONSUME
        end
    end

    local source_lines, starts = split_document(text)
    local original_count = #source_lines
    while #source_lines < target_line do source_lines[#source_lines + 1] = "" end

    local first_line = math.min(line, target_line)
    local last_line = math.max(line, target_line)
    local through_column = math.max(column, target_column)
    local editable = {}
    for index = first_line, last_line do
        local character_error
        editable[index], character_error = characters(
            source_lines[index], tab_width, through_column
        )
        if editable[index] == nil then
            mc.ui.status("Box drawing: " .. (character_error or "invalid UTF-8 text"))
            return mc.CONSUME
        end
    end

    if not add_connection(editable[line], column, direction.here, style)
        or not add_connection(editable[target_line], target_column, direction.there, style) then
        mc.ui.status("Box drawing: cursor is inside a wide character")
        return mc.CONSUME
    end

    local replacement_lines = {}
    for index = first_line, last_line do
        replacement_lines[#replacement_lines + 1] = table.concat(editable[index])
    end

    local from = starts[first_line] or #text
    local to = last_line <= original_count
        and starts[last_line] + #source_lines[last_line]
        or #text
    local replacement = table.concat(replacement_lines, "\n")
    local edit_result, replace_error = editor:replace({
        from = from, to = to, revision = info.revision,
    }, replacement)
    if edit_result == nil then
        mc.ui.status("Box drawing: " .. replace_error)
        return mc.CONSUME
    end

    local ok, cursor_error = editor:set_cursor(target_line, target_column)
    if not ok then mc.ui.status("Box drawing: " .. cursor_error) end
    return mc.CONSUME
end

local function register_mode_action(id, description, menu_label, position, style)
    mc.macro {
        id = id,
        area = "editor",
        description = description,
        menu = {
            path = "Drawing",
            label = menu_label,
            position = position,
        },
        action = function()
            active_style = style
            if active_style == "single" then
                mc.ui.indicator {
                    id = indicator_id,
                    area = "editor",
                    text = "┌─┐",
                    priority = 100,
                }
            elseif active_style == "double" then
                mc.ui.indicator {
                    id = indicator_id,
                    area = "editor",
                    text = "╔═╗",
                    priority = 100,
                }
            else
                mc.ui.indicator_clear(indicator_id)
            end
            mc.ui.status(description)
            return mc.CONSUME
        end,
    }
end

register_mode_action(
    "box-drawing-single-enable", "Enable single box drawing", "Draw single line", 10,
    "single"
)
register_mode_action(
    "box-drawing-double-enable", "Enable double box drawing", "Draw double line", 20,
    "double"
)
register_mode_action(
    "box-drawing-stop", "Disable box drawing", "Stop line drawing", 30,
    nil
)

local bindings = {
    { "shift-left", "Shift-Left", "left" },
    { "shift-right", "Shift-Right", "right" },
    { "shift-up", "Shift-Up", "up" },
    { "shift-down", "Shift-Down", "down" },
}

for _, binding in ipairs(bindings) do
    local id, key, direction = table.unpack(binding)
    mc.macro {
        id = "box-drawing-" .. id,
        area = "editor",
        key = key,
        description = "Draw active box line " .. direction,
        listed = false,
        action = function(ev) return draw(ev.editor, direction) end,
    }
end

mc.macro {
    id = "box-drawing-expand",
    area = "editor",
    key = "Space",
    description = "Widen the table under the cursor",
    listed = false,
    action = function(ev) return expand(ev.editor) end,
}

-- The runtime names the Enter key by either alias of its key code.
for _, binding in ipairs({ { "box-drawing-grow", "Enter" }, { "box-drawing-grow-kp", "kpenter" } }) do
    local id, key = table.unpack(binding)
    mc.macro {
        id = id,
        area = "editor",
        key = key,
        description = "Add a row to the table below the cursor",
        listed = false,
        action = function(ev) return grow(ev.editor) end,
    }
end
