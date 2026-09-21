-- The settings a reader may change, and the file they are kept in.  One
-- table describes every setting: the dialog builds its controls from it, and
-- the file is read and written through it as well, so a new setting is added
-- in one place.

local cfg = require("config")

local M = {}

-- Every setting: the name it has in config.lua, what it holds, and what the
-- dialog calls it.  A "choice" is picked from the list under it; a choice
-- keeps the value it stands for in "value", because the dialog needs an id
-- and an id is never empty.
M.ITEMS = {
    {
        key = "MAX_WIDTH", kind = "number", min = 40, max = 512,
        label = "Widest the text is flowed, in columns:",
    },
    {
        key = "CODE_FRAME", kind = "boolean",
        label = "Frame a code block and name its language",
    },
    {
        key = "CODE_BG", kind = "choice",
        label = "Background of a code block:",
        choices = {
            { id = "auto", value = "auto", label = "A shade of the one the skin paints" },
            { id = "none", value = "", label = "None, as the terminal has it" },
            { id = "custom", label = "The SGR written below" },
        },
        -- the choice whose value the reader types instead of picking
        free_id = "custom",
    },
    {
        key = "CODE_BG_SHIFT", kind = "number", min = 0, max = 128,
        label = "How far that shade moves, of 255:",
    },
    {
        key = "CODE_MIN", kind = "number", min = 20, max = 200,
        label = "The narrowest a code block gets, in columns:",
    },
    {
        key = "DECISION_STYLE", kind = "choice",
        label = "A decision in a diagram is drawn with:",
        choices = {
            { id = "braille", label = "Sloping sides, out of braille dots" },
            { id = "box", label = "Box drawing characters" },
        },
    },
    {
        key = "DIAGRAM_WIDTH", kind = "number", min = 40, max = 512,
        label = "A diagram is laid out up to this width, in columns:",
    },
}

-- The file the settings are kept in, next to the other configuration of mc.
local function config_dir()
    local xdg = os.getenv("XDG_CONFIG_HOME")

    if xdg ~= nil and xdg ~= "" then
        return xdg .. "/mc6"
    end
    local home = os.getenv("HOME")

    if home == nil or home == "" then
        return nil
    end
    return home .. "/.config/mc6"
end

function M.path()
    local dir = config_dir()

    return dir ~= nil and (dir .. "/lua-markdown.ini") or nil
end

local function item_of(key)
    for _, item in ipairs(M.ITEMS) do
        if item.key == key then
            return item
        end
    end
    return nil
end

-- What the file says for one setting, turned into the value config.lua holds.
local function value_of(item, text)
    if item.kind == "number" then
        local n = math.tointeger(tonumber(text))

        if n == nil then
            return nil
        end
        return math.max(item.min, math.min(item.max, n))
    end
    if item.kind == "boolean" then
        if text == "true" or text == "yes" or text == "1" then
            return true
        end
        if text == "false" or text == "no" or text == "0" then
            return false
        end
        return nil
    end
    return text
end

local function text_of(item, value)
    if item.kind == "boolean" then
        return value and "true" or "false"
    end
    return tostring(value)
end

-- Read the file into config.lua.  A line it does not know is left alone, so
-- that a file written by a later version still loads.
function M.load()
    local path = M.path()

    if path == nil then
        return false
    end
    local file = io.open(path, "r")

    if file == nil then
        return false
    end
    for line in file:lines() do
        local key, text = line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
        local item = key ~= nil and item_of(key) or nil

        if item ~= nil then
            local value = value_of(item, text)

            if value ~= nil then
                cfg[item.key] = value
            end
        end
    end
    file:close()
    return true
end

-- Write what config.lua holds now.  The file is small and written whole, so
-- that a setting taken out of the dialog leaves no line behind.
function M.save()
    local path = M.path()

    if path == nil then
        return nil, "no configuration directory"
    end
    local file, err = io.open(path, "w")

    if file == nil then
        -- the configuration directory may not be there on a first run
        os.execute("mkdir -p '" .. config_dir():gsub("'", "'\\''") .. "'")
        file, err = io.open(path, "w")
    end
    if file == nil then
        return nil, err
    end
    file:write("# The markdown viewer of mc.  Written by its settings dialog.\n")
    for _, item in ipairs(M.ITEMS) do
        file:write(item.key, " = ", text_of(item, cfg[item.key]), "\n")
    end
    file:close()
    return true
end

------------------------------------------------------------------------
-- The dialog, built out of the same table.

-- The controls of one setting.  A choice the reader may type a value into
-- is a select with an input under it.
local function controls_of(item, controls)
    local value = cfg[item.key]

    if item.kind == "boolean" then
        controls[#controls + 1] =
            { id = item.key, type = "checkbox", label = item.label, value = value and true or false }
        return
    end
    if item.kind == "number" then
        controls[#controls + 1] = { type = "label", text = item.label }
        controls[#controls + 1] =
            { id = item.key, type = "input", value = tostring(value), width = 8 }
        return
    end

    local options = {}
    local chosen = nil

    for _, choice in ipairs(item.choices) do
        options[#options + 1] = { id = choice.id, label = choice.label }
        if choice.id ~= item.free_id and (choice.value or choice.id) == value then
            chosen = choice.id
        end
    end
    -- a value the list does not hold is one the reader typed before
    if chosen == nil then
        chosen = item.free_id
    end
    controls[#controls + 1] =
        { id = item.key, type = "select", label = item.label, value = chosen, options = options }
    if item.free_id ~= nil then
        controls[#controls + 1] = {
            id = item.key .. "_TEXT", type = "input", width = 16,
            value = chosen == item.free_id and tostring(value) or "",
        }
    end
end

-- What the reader answered, back into config.lua.  A number that does not
-- read as one leaves its setting as it was.
local function apply(values)
    for _, item in ipairs(M.ITEMS) do
        local answer = values[item.key]

        if item.kind == "boolean" then
            cfg[item.key] = answer and true or false
        elseif item.kind == "number" then
            local n = value_of(item, answer)

            if n ~= nil then
                cfg[item.key] = n
            end
        elseif item.free_id ~= nil and answer == item.free_id then
            cfg[item.key] = values[item.key .. "_TEXT"] or ""
        else
            for _, choice in ipairs(item.choices) do
                if choice.id == answer then
                    cfg[item.key] = choice.value or choice.id
                end
            end
        end
    end
end

-- Ask for the settings and keep them.  True when something may have
-- changed, so that the caller renders the document again.
function M.dialog()
    local controls = {}

    for _, item in ipairs(M.ITEMS) do
        controls_of(item, controls)
    end
    controls[#controls + 1] = { type = "separator" }
    controls[#controls + 1] = { type = "label", text = "Kept in " .. (M.path() or "nowhere") }
    controls[#controls + 1] = { type = "hbox", expand_x = true, controls = {
        { type = "spacer", expand_x = true },
        { id = "ok", type = "button", label = "&OK", default = true },
        { id = "cancel", type = "button", label = "&Cancel", cancel = true },
    } }

    local result, err = mc.ui.dialog {
        title = "Markdown viewer settings",
        width = 64,
        help = { file = "help.hlp", node = "[Markdown viewer settings]" },
        controls = controls,
    }
    if result == nil then
        if err ~= nil and err ~= "cancelled" then
            mc.ui.message("Markdown viewer", err)
        end
        return false
    end
    apply(result.values)

    local saved, save_error = M.save()

    if saved == nil then
        mc.ui.message("Markdown viewer", "The settings are not kept: " .. tostring(save_error))
    end
    return true
end

return M
