-- F3 on a markdown file: lib/render.lua turns the text into the
-- nroff-style text the viewer paints: headings and **bold** as overstruck
-- letters, `code`, *italic* and links as underlined, $LaTeX$ as Unicode
-- symbols, tables aligned in columns with box-drawing rules.  F8 shows the
-- file itself.

local MAX_FILE_SIZE = 16 * 1024 * 1024

local md = require("render")

------------------------------------------------------------------------
-- F3 on a .md file.

-- Paragraphs are wrapped to the width of the viewer, up to md.MAX_WIDTH
-- columns on a wide screen, so the text is rendered again when the window
-- changes size; a width seen before is served from the session.
local viewer = mc.viewer_source.define {
    id = "markdown",
    resize = "rebuild",
    open = function(request)
        request.rendered = {}
        return request
    end,
    prepare = function(session, _, viewport)
        local width = math.min(viewport.columns, md.MAX_WIDTH)
        local source
        if session.rendered[width] ~= nil then
            source = mc.source.bytes(session.rendered[width])
        elseif mc.source.generator == nil then
            session.rendered[width] = md.render(session.text, { width = width })
            source = mc.source.bytes(session.rendered[width])
        else
            local next_block = md.blocks(session.text, { width = width })
            local pieces = {}
            local lines = 0
            local done = false
            -- Fill the first screen before handing the rest to the event loop.
            repeat
                local chunk = next_block()
                if chunk == nil then
                    done = true
                else
                    pieces[#pieces + 1] = chunk
                    lines = lines + select(2, chunk:gsub("\n", ""))
                end
            until done or lines >= viewport.lines
            local initial = table.concat(pieces)
            if done then
                session.rendered[width] = initial
                source = mc.source.bytes(initial)
            else
                source = mc.source.generator {
                    initial = initial,
                    next = function()
                        local chunk = next_block()
                        if chunk ~= nil then
                            pieces[#pieces + 1] = chunk
                        else
                            session.rendered[width] = table.concat(pieces)
                            pieces = nil
                        end
                        return chunk
                    end,
                }
            end
        end
        return {
            source = source,
            title = session.title,
            raw_path = session.raw_path,
            initial_display = "nroff",
            auto_scroll = "top",
        }
    end,
    close = function() end,
}

local function read_file(path)
    local file, err = io.open(path, "rb")
    if file == nil then
        return nil, err
    end
    local size = file:seek("end")
    if size == nil or size > MAX_FILE_SIZE then
        file:close()
        return nil, "too large"
    end
    file:seek("set", 0)
    local text = file:read("a")
    file:close()
    if text == nil then
        return nil, "cannot read"
    end
    return text
end

local function view_file(request)
    if request.local_path == nil then
        return nil, "not_supported"
    end
    local text, err = read_file(request.local_path)
    if text == nil then
        mc.log.info(request.display_name .. ": " .. err)
        return nil, "not_supported"
    end
    local controller, create_err = viewer:create {
        text = text,
        title = request.display_name,
        raw_path = request.local_path,
    }
    if controller == nil then
        return nil, create_err
    end
    return { handled = true, controller = controller }
end

mc.file_handler.register { id = "view", kind = "view", handler = view_file }
