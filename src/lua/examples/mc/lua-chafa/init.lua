local viewer = mc.viewer_source.define {
    id = "chafa-image",
    resize = "rebuild",

    open = function(request)
        return {
            local_path = request.local_path,
            display_name = request.display_name,
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
            initial_display = "terminal",
            auto_scroll = "top",
        }
    end,

    close = function(_) end,
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
