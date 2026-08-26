local next_session_id = 0

-- Image properties the way misc/ext.d/image.sh printed them on F3
-- (exif, or exiftool when exif is missing or fails, then identify),
-- followed by the chafa rendering.
local render_script = [[
file=$1
size=$2

if command -v exif >/dev/null 2>&1; then
    exif "$file" 2>/dev/null
    E=$?
else
    E=1
fi
if [ $E != 0 ] && command -v exiftool >/dev/null 2>&1; then
    exiftool "$file" 2>/dev/null
fi
if command -v identify >/dev/null 2>&1; then
    # first frame only: an animated GIF has one line per frame
    identify -ping -format '%m %wx%h %z-bit, %n frame(s)\n' "$file" 2>/dev/null | head -n 1
fi
echo

exec chafa --format=symbols --colors=256 --animate=off --size="$size" -- "$file"
]]

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
                    "sh",
                    "-c",
                    render_script,
                    "lua-chafa",
                    session.local_path,
                    viewport.columns .. "x" .. viewport.lines,
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
