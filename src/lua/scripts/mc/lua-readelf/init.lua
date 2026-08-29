local next_session_id = 0

local viewer = mc.viewer_source.define {
    id = "readelf-elf",

    open = function(request)
        next_session_id = next_session_id + 1
        return {
            local_path = request.local_path,
            display_name = request.display_name,
            indicator_id = "readelf-" .. next_session_id,
        }
    end,

    prepare = function(session)
        return {
            source = mc.source.process {
                argv = {
                    "readelf",
                    "-W",
                    "-h",
                    "-S",
                    "-l",
                    "-d",
                    "--",
                    session.local_path,
                },
                stderr = "separate",
            },
            title = session.display_name,
            raw_path = session.local_path,
            initial_display = "text",
            auto_scroll = "top",
        }
    end,

    source_state = function(session, event)
        if event.state == "started" then
            session.generation = event.generation
            mc.ui.indicator {
                id = session.indicator_id,
                area = "viewer",
                text = "Reading ELF metadata...",
            }
        elseif session.generation == event.generation then
            if event.state == "failed" or (event.state == "finished" and event.output_size == 0) then
                mc.ui.indicator {
                    id = session.indicator_id,
                    area = "viewer",
                    text = session.display_name .. " (readelf error)",
                }
            else
                mc.ui.indicator_clear(session.indicator_id, "viewer")
            end
            session.generation = nil
        end
    end,

    close = function(session)
        mc.ui.indicator_clear(session.indicator_id, "viewer")
    end,
}

mc.file_handler.register {
    id = "view",
    kind = "view",
    handler = function(request)
        local controller, err

        controller, err = viewer:create(request)
        if controller == nil then
            return nil, err
        end

        return {
            handled = true,
            controller = controller,
        }
    end,
}
