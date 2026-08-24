local next_session_id = 0

local viewer = mc.viewer_source.define {
    id = "chafa-image",
    resize = "rebuild",

    open = function(request)
        next_session_id = next_session_id + 1
        return {
            local_path = request.local_path,
            display_name = request.display_name,
            indicator_id = "render-" .. next_session_id,
        }
    end,

    prepare = function(session, _, viewport)
        return {
            source = mc.source.process {
                argv = {
                    "chafa",
                    "--format=symbols",
                    "--colors=256",
                    "--size=" .. viewport.columns .. "x" .. viewport.lines,
                    "--",
                    session.local_path,
                },
                stderr = "separate",
            },
            title = session.display_name,
            raw_path = session.local_path,
            initial_display = "terminal",
            auto_scroll = "top",
        }
    end,

    source_state = function(session, event)
        if event.state == "started" then
            session.generation = event.generation
            mc.ui.indicator {
                id = session.indicator_id,
                area = "viewer",
                text = "Rendering image...",
            }
        elseif session.generation == event.generation then
            mc.ui.indicator_clear(session.indicator_id, "viewer")
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
        return {
            handled = true,
            controller = viewer:create(request),
        }
    end,
}
