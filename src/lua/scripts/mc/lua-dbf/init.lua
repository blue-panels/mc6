-- F3 or Enter on a DBF file: a full-screen table, one record per row and
-- one column per field, decoded here, in Lua, and served to the screen
-- page by page as it scrolls.  Enter or F3 shows the record as a card;
-- F2 the structure of the table;
-- F4 hides or shows the deleted records; F5 picks the encoding; F8 shows
-- the file itself.

local PAGE_SIZE = 256

------------------------------------------------------------------------
-- Encodings: single-byte code pages to UTF-8.

local function cyrillic_range(first_byte, first_code, count)
    local map = {}
    for i = 0, count - 1 do
        map[first_byte + i] = utf8.char(first_code + i)
    end
    return map
end

local function merge(into, from)
    for byte, text in pairs(from) do
        into[byte] = text
    end
    return into
end

local function code_points(first_byte, list)
    local map = {}
    for i, code in ipairs(list) do
        if code ~= 0 then
            map[first_byte + i - 1] = utf8.char(code)
        end
    end
    return map
end

local cp866 = merge(merge(cyrillic_range(0x80, 0x0410, 48), cyrillic_range(0xE0, 0x0440, 16)),
    code_points(0xB0, {
        0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
        0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
        0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
        0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
        0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
        0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    }))
merge(cp866, code_points(0xF0, {
    0x0401, 0x0451, 0x0404, 0x0454, 0x0407, 0x0457, 0x040E, 0x045E,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x2116, 0x00A4, 0x25A0, 0x00A0,
}))

local cp1251 = merge(cyrillic_range(0xC0, 0x0410, 64), code_points(0x80, {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
}))

local cp1252 = code_points(0x80, {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
})
for byte = 0xA0, 0xFF do
    cp1252[byte] = utf8.char(byte)
end

local code_pages = { cp866 = cp866, cp1251 = cp1251, cp1252 = cp1252 }

-- The encoding named by the language driver byte of the header.
local language_drivers = {
    [0x01] = "cp1252", [0x02] = "cp1252", [0x03] = "cp1252", [0x57] = "cp1252",
    [0x64] = "cp1252", [0x65] = "cp866", [0x66] = "cp1252", [0x67] = "cp1252",
    [0xC8] = "cp1252", [0xC9] = "cp1251", [0x26] = "cp866",
}

local function decoder(encoding)
    local map = code_pages[encoding]
    if map == nil then
        return function(text)
            return text
        end
    end
    return function(text)
        return (text:gsub("[\128-\255]", function(byte)
            return map[byte:byte()] or "?"
        end))
    end
end

------------------------------------------------------------------------
-- The DBF file.

local versions = {
    [0x02] = "FoxBASE", [0x03] = "dBase III", [0x04] = "dBase IV", [0x05] = "dBase V",
    [0x30] = "Visual FoxPro", [0x31] = "Visual FoxPro, autoincrement",
    [0x32] = "Visual FoxPro, varchar", [0x43] = "dBase IV SQL table",
    [0x63] = "dBase IV SQL system", [0x83] = "dBase III with memo",
    [0x8B] = "dBase IV with memo", [0x8E] = "dBase IV SQL table with memo",
    [0xF5] = "FoxPro 2.x with memo", [0xFB] = "FoxBASE",
}

local field_types = {
    C = "character", N = "numeric", F = "float", D = "date", L = "logical",
    M = "memo", I = "integer", T = "datetime", Y = "currency", B = "double",
    G = "general", P = "picture", V = "varchar", Q = "varbinary", W = "blob",
    ["0"] = "null flags", ["+"] = "autoincrement", ["@"] = "timestamp",
}

-- The header and the field list; the records are read later, on demand.
local function read_header(path)
    local file, err = io.open(path, "rb")
    if file == nil then
        return nil, err
    end
    local head = file:read(32)
    if head == nil or #head < 32 then
        file:close()
        return nil, "the file is shorter than a DBF header"
    end
    local version, yy, mm, dd, record_count, header_length, record_length =
        string.unpack("<BBBBI4I2I2", head)
    local language = head:byte(30)

    local fields = {}
    local offset = 2 -- the first byte of a record is its deleted mark
    while true do
        local descriptor = file:read(32)
        if descriptor == nil or descriptor:byte(1) == 0x0D or #fields >= 255 then
            break
        end
        if #descriptor < 32 then
            break
        end
        local name = descriptor:sub(1, 11):match("^([^%z]*)")
        local length = descriptor:byte(17)
        local decimals = descriptor:byte(18)
        fields[#fields + 1] = {
            name = name,
            type = descriptor:sub(12, 12),
            length = length,
            decimals = decimals,
            offset = offset,
        }
        offset = offset + length
    end
    file:close()

    -- What every DBF writer agrees on: the version byte from dBase II
    -- through Visual FoxPro, a header of the descriptors read (Visual
    -- FoxPro adds a 263-byte backlink), a record of the fields read.
    local known_version = versions[version] ~= nil or (version & 0x07) >= 2
    local descriptors_length = 32 + 32 * #fields + 1
    if #fields == 0 or not known_version
        or (header_length ~= descriptors_length and header_length ~= descriptors_length + 263)
        or record_length ~= offset - 1 then
        return nil, "not a DBF file"
    end
    for _, field in ipairs(fields) do
        if field_types[field.type] == nil or not field.name:match("^[%w_]+$") then
            return nil, "not a DBF file: field " .. field.name .. " of type " .. field.type
        end
    end
    return {
        path = path,
        version = version,
        version_name = versions[version],
        updated = string.format("%04d-%02d-%02d", 1900 + yy, mm, dd),
        record_count = record_count,
        header_length = header_length,
        record_length = record_length,
        language = language,
        encoding = language_drivers[language],
        fields = fields,
    }
end

local function trim(text)
    return (text:gsub("^%s+", ""):gsub("%s+$", ""))
end

-- One field of one record as text, and whether it stands to the right.
local function field_text(field, raw, decode)
    local t = field.type
    if t == "C" or t == "V" then
        return decode(trim(raw:gsub("%z", ""))), false
    elseif t == "N" or t == "F" then
        return trim(raw), true
    elseif t == "D" then
        local y, m, d = raw:match("^(%d%d%d%d)(%d%d)(%d%d)$")
        if y then
            return y .. "-" .. m .. "-" .. d, false
        end
        return trim(raw), false
    elseif t == "L" then
        local c = raw:sub(1, 1):upper()
        if c == "T" or c == "Y" then
            return "T", false
        elseif c == "F" or c == "N" then
            return "F", false
        end
        return "?", false
    elseif t == "M" or t == "G" or t == "P" or t == "W" then
        if #raw == 4 then
            local block = string.unpack("<I4", raw)
            return block ~= 0 and ("<memo " .. block .. ">") or "", false
        end
        local block = trim(raw)
        return block ~= "" and ("<memo " .. block .. ">") or "", false
    elseif (t == "I" or t == "+") and #raw == 4 then
        return tostring(string.unpack("<i4", raw)), true
    elseif t == "Y" and #raw == 8 then
        return string.format("%.4f", string.unpack("<i8", raw) / 10000), true
    elseif t == "B" and #raw == 8 then
        return string.format("%.15g", string.unpack("<d", raw)), true
    elseif (t == "T" or t == "@") and #raw == 8 then
        local julian, ms = string.unpack("<I4I4", raw)
        if julian == 0 then
            return "", false
        end
        -- Julian day to the civil date, Fliegel and Van Flandern.
        local l = julian + 68569
        local n = 4 * l // 146097
        l = l - (146097 * n + 3) // 4
        local i = 4000 * (l + 1) // 1461001
        l = l - 1461 * i // 4 + 31
        local j = 80 * l // 2447
        local day = l - 2447 * j // 80
        l = j // 11
        local month = j + 2 - 12 * l
        local year = 100 * (n - 49) + i + l
        local seconds = ms // 1000
        return string.format("%04d-%02d-%02d %02d:%02d:%02d", year, month, day,
            seconds // 3600, seconds // 60 % 60, seconds % 60), false
    end
    return decode(trim(raw:gsub("%z", ""))), false
end

------------------------------------------------------------------------
-- Text views: a record card and the structure.

local function width_of(text)
    return utf8.len(text) or #text
end

local function pad(text, width, right)
    local fill = width - width_of(text)
    if fill <= 0 then
        return text
    end
    if right then
        return string.rep(" ", fill) .. text
    end
    return text .. string.rep(" ", fill)
end

local function render_card(info, number, record)
    local name_width = 0
    for _, field in ipairs(info.fields) do
        name_width = math.max(name_width, width_of(field.name))
    end
    local out = { string.format("Record %d of %d%s", number, info.record_count,
        record.deleted and " (deleted)" or ""), "" }
    for i, field in ipairs(info.fields) do
        out[#out + 1] = pad(field.name, name_width, false) .. ": " .. record.values[i]
    end
    return table.concat(out, "\n") .. "\n"
end

local function render_structure(info)
    local out = {
        "File:           " .. info.path,
        string.format("Version:        0x%02X %s", info.version, info.version_name or "(unknown)"),
        "Last update:    " .. info.updated,
        "Records:        " .. info.record_count,
        "Header length:  " .. info.header_length,
        "Record length:  " .. info.record_length,
        string.format("Language:       0x%02X %s", info.language, info.encoding or "(unknown)"),
        "",
        "  # Name        Type            Length  Dec",
        "--- ----------- --------------- ------  ---",
    }
    for i, field in ipairs(info.fields) do
        out[#out + 1] = string.format("%3d %-11s %-1s %-13s %6d  %3d", i, field.name, field.type,
            field_types[field.type] or "?", field.length, field.decimals)
    end
    return table.concat(out, "\n") .. "\n"
end

local text_viewer = mc.viewer_source.define {
    id = "dbf-text",
    help = { file = "help.hlp", node = "[DBF Viewer]" },
    open = function(request)
        return { text = request.text, title = request.title, raw_path = request.raw_path }
    end,
    prepare = function(session)
        return {
            source = mc.source.bytes(session.text),
            title = session.title,
            raw_path = session.raw_path,
            initial_display = "text",
            auto_scroll = "top",
        }
    end,
    close = function() end,
}

local function show_text(text, title, raw_path)
    local controller, err = text_viewer:create { text = text, title = title, raw_path = raw_path }
    if controller == nil then
        return nil, err
    end
    return mc.ui.open_viewer { controller = controller }
end

------------------------------------------------------------------------
-- The screen: a table of the records, served page by page.

local function encoding_of(session)
    if session.encoding == "auto" then
        return session.info.encoding or "utf8"
    end
    return session.encoding
end

local function is_right_aligned(field)
    local t = field.type
    return t == "N" or t == "F" or t == "I" or t == "Y" or t == "B" or t == "+"
end

-- The record numbers shown, in order: every record, or the ones not
-- deleted.  One pass over the file, a chunk at a time, looking at the
-- first byte of each record only.
local function live_records(info)
    local file = io.open(info.path, "rb")
    if file == nil then
        return {}
    end
    local live = {}
    local number = 0
    file:seek("set", info.header_length)
    local chunk_records = math.max(1, 65536 // info.record_length)
    while number < info.record_count do
        local data = file:read(chunk_records * info.record_length)
        if data == nil or #data == 0 then
            break
        end
        for offset = 1, #data - info.record_length + 1, info.record_length do
            number = number + 1
            if number > info.record_count then
                break
            end
            local mark = data:byte(offset)
            if mark == 0x1A then
                number = info.record_count
                break
            end
            if mark ~= 0x2A then -- '*'
                live[#live + 1] = number
            end
        end
    end
    file:close()
    return live
end

-- The record with this number (from 1), or nil.
local function read_record(session, number)
    local info = session.info
    if number < 1 or number > info.record_count then
        return nil
    end
    if session.file == nil then
        session.file = io.open(info.path, "rb")
        if session.file == nil then
            return nil
        end
    end
    session.file:seek("set", info.header_length + (number - 1) * info.record_length)
    local raw = session.file:read(info.record_length)
    if raw == nil or #raw < info.record_length then
        return nil
    end
    local decode = decoder(encoding_of(session))
    local values = {}
    for i, field in ipairs(info.fields) do
        values[i] = field_text(field, raw:sub(field.offset, field.offset + field.length - 1), decode)
    end
    return { number = number, deleted = raw:sub(1, 1) == "*", values = values }
end

-- The record number of a screen row (from 0).
local function row_number(session, row)
    if session.live ~= nil then
        return session.live[row + 1]
    end
    return row + 1
end

local function row_count(session)
    if session.live ~= nil then
        return #session.live
    end
    return session.info.record_count
end

local function status_text(session)
    local info = session.info
    local shown = row_count(session)
    local text = string.format("%s: %d fields, %d records", session.display_name, #info.fields,
        info.record_count)
    if shown ~= info.record_count then
        text = text .. string.format(", %d shown", shown)
    end
    return text .. ", " .. encoding_of(session) .. (session.deleted and "" or ", deleted hidden")
end

-- Rows first..first+count-1 (from 0) as cells: the record number, then the
-- fields; a deleted record is drawn in red.
local function screen_rows(session, first, count)
    local rows = {}
    local last = math.min(first + count, row_count(session)) - 1
    for row = first, last do
        local record = read_record(session, row_number(session, row))
        if record == nil then
            break
        end
        -- the deleted mark the way the file keeps it, then the number, then the fields
        local cells = { record.deleted and "*" or " ", tostring(record.number) }
        for i, value in ipairs(record.values) do
            cells[i + 2] = value
        end
        if record.deleted then
            for i, value in ipairs(cells) do
                cells[i] = { text = value, color = "red" }
            end
        end
        rows[#rows + 1] = cells
    end
    return rows
end

local function screen_columns(info)
    local columns = {
        { id = "del", title = "", min_width = 1 },
        { id = "rec", title = "#", align = "right", min_width = #tostring(info.record_count) },
    }
    for i, field in ipairs(info.fields) do
        local width = field.length
        if field.type == "D" then
            width = 10
        elseif field.type == "T" or field.type == "@" then
            width = 19
        elseif field.type == "M" or field.type == "G" or field.type == "P" or field.type == "W" then
            width = 10
        elseif field.type == "I" or field.type == "+" then
            width = 11
        elseif field.type == "Y" or field.type == "B" then
            width = 16
        end
        columns[i + 2] = {
            id = "f" .. i,
            title = field.name,
            align = is_right_aligned(field) and "right" or "left",
            min_width = math.min(math.max(width, width_of(field.name)), 40),
        }
    end
    return columns
end

local function show_card(session, row)
    if row == nil then
        return
    end
    local number = row_number(session, row)
    local record = number and read_record(session, number)
    if record == nil then
        return
    end
    show_text(render_card(session.info, number, record), session.display_name .. " #" .. number,
        session.info.path)
end

local raw_viewer = mc.viewer_source.define {
    id = "dbf-raw",
    open = function(request)
        return { path = request.path, title = request.title }
    end,
    prepare = function(s)
        return { source = mc.source.file { path = s.path }, title = s.title, initial_display = "text" }
    end,
    close = function() end,
}

local function show_raw(session)
    local controller = raw_viewer:create { path = session.info.path, title = session.display_name }
    if controller ~= nil then
        mc.ui.open_viewer { controller = controller }
    end
end

local function choose_encoding(session)
    local result, err = mc.ui.dialog {
        title = "DBF encoding",
        width = 44,
        controls = {
            { id = "encoding", type = "select", label = "Encoding:", value = session.encoding,
              options = {
                  { id = "auto", label = "By the language driver" },
                  { id = "cp866", label = "cp866 (DOS)" },
                  { id = "cp1251", label = "cp1251 (Windows)" },
                  { id = "cp1252", label = "cp1252 (Western)" },
                  { id = "utf8", label = "UTF-8, as is" },
              } },
            { type = "hbox", expand_x = true, controls = {
                { type = "spacer", expand_x = true },
                { id = "ok", type = "button", label = "&OK", default = true },
                { id = "cancel", type = "button", label = "&Cancel", cancel = true },
            } },
        },
    }
    if result == nil then
        if err ~= "cancelled" then
            mc.ui.message("DBF viewer", err)
        end
        return false
    end
    session.encoding = result.values.encoding
    return true
end

local function open_screen(info, display_name)
    -- every record, the deleted ones marked and red; F4 hides them
    local session = {
        info = info,
        display_name = display_name,
        encoding = "auto",
        deleted = true,
        live = nil,
    }

    local screen = mc.ui.screen {
        title = display_name,
        status = status_text(session),
        help = { file = "help.hlp", node = "[DBF Viewer]" },
        layout = {
            { weight = 1,
              { weight = 1, id = "grid", type = "table",
                columns = screen_columns(info),
                row_count = row_count(session),
                page_size = PAGE_SIZE,
                rows = function(first, count)
                    return screen_rows(session, first, count)
                end } },
        },
        keys = {
            { key = "f1", label = "Help", action = "help" },
            { key = "f2", label = "Struct", action = "structure" },
            { key = "f3", label = "Card", action = "card" },
            { key = "f4", label = "Deleted", action = "deleted" },  -- show or hide
            { key = "f5", label = "Encoding", action = "encoding" },
            { key = "f8", label = "Raw", action = "raw" },
            { key = "f10", label = "Quit", action = "close" },
        },
        on_enter = function(scr, ev)
            show_card(session, ev.row)
        end,
        on_action = function(scr, id, ev)
            if id == "structure" then
                show_text(render_structure(info), display_name .. " structure", info.path)
            elseif id == "card" then
                show_card(session, ev.row)
            elseif id == "raw" then
                show_raw(session)
            elseif id == "deleted" then
                session.deleted = not session.deleted
                if session.deleted then
                    session.live = nil
                else
                    session.live = live_records(info)
                    if #session.live == info.record_count then
                        session.live = nil
                    end
                end
                scr:update("grid", { row_count = row_count(session), invalidate = true, row = 0 })
                scr:status(status_text(session))
            elseif id == "encoding" then
                if choose_encoding(session) then
                    scr:update("grid", { invalidate = true })
                    scr:status(status_text(session))
                end
            end
        end,
        on_close = function()
            if session.file ~= nil then
                session.file:close()
                session.file = nil
            end
        end,
    }
    if screen == nil then
        return false
    end
    return screen:run()
end

------------------------------------------------------------------------
-- F3 and Enter on a .dbf file.

local function view_file(request)
    if request.local_path == nil then
        return nil, "not_supported"
    end
    local info, err = read_header(request.local_path)
    if info == nil then
        mc.log.info(request.display_name .. ": " .. err)
        return nil, "not_supported"
    end
    local ok, run_err = open_screen(info, request.display_name)
    if not ok then
        return nil, run_err or "cannot open the screen"
    end
    return { handled = true }
end

mc.file_handler.register { id = "view", kind = "view", handler = view_file }
mc.file_handler.register { id = "open", kind = "open", handler = view_file }
