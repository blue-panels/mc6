-- F3 on a picture: sixel where the terminal draws it, chafa's symbols where
-- it does not. Alt-O switches between the two.

local next_session_id = 0

-- What misc/ext.d/image.sh printed on F3 (exif, or exiftool when exif is
-- missing or fails, then one line of identify), followed by the picture in
-- the rows left under them: a picture that does not fit whole is not drawn.
-- Without chafa there is no picture, and a line says what to install.
--
-- $1 file, $2 "sixel" or "symbols", $3 columns, $4 lines, $5 and $6 the cell
-- in pixels (sixel), $7... the rest of chafa's arguments.
local render_script = [[
file=$1
mode=$2
cols=$3
lines=$4
cell_w=$5
cell_h=$6
shift 6

props=
if command -v exif >/dev/null 2>&1; then
    props=$(exif "$file" 2>/dev/null)
fi
if [ -z "$props" ] && command -v exiftool >/dev/null 2>&1; then
    props=$(exiftool "$file" 2>/dev/null)
fi
if command -v identify >/dev/null 2>&1; then
    line=$(identify -ping -format '%m %wx%h %z-bit, %n frame(s)\n' "$file" 2>/dev/null | head -n 1)
    [ -n "$line" ] && props="${props:+$props
}$line"
fi

used=0
if [ -n "$props" ]; then
    printf '%s\n\n' "$props"
    used=$(($(printf '%s\n' "$props" | wc -l) + 1))
fi
rows=$((lines - used))
[ "$rows" -lt 1 ] && rows=1

if ! command -v chafa >/dev/null 2>&1; then
    echo "Install chafa to see the picture here."
    exit 0
fi

if [ "$mode" = sixel ]; then
    # chafa with no terminal to ask takes a cell for 8x8 pixels under
    # --font-ratio=1/1: the size in its cells is the size in pixels over 8.
    size=$((cols * cell_w / 8))x$((rows * cell_h / 8))
    set -- --font-ratio=1/1 "$@"
else
    size=${cols}x${rows}
fi

exec chafa --format="$mode" --animate=off --size="$size" "$@" -- "$file"
]]

-- Whether the picture is drawn in sixel: by the mode chosen, and by whether
-- the terminal draws it, which the viewport says by carrying pixels.
local function want_sixel(params, viewport)
    if params.mode == "symbols" then
        return false
    end
    if params.mode == "sixel" then
        return true
    end
    return viewport.pixel_width ~= nil
end

local viewer = mc.viewer_source.define {
    id = "sixel-image",
    resize = "rebuild",
    options_key = "alt-o",

    open = function(request)
        next_session_id = next_session_id + 1
        return {
            local_path = request.local_path,
            display_name = request.display_name,
            indicator_id = "render-" .. next_session_id,
        }
    end,

    initial_params = function(_, params)
        params.mode = params.mode or "auto"
        return params
    end,

    prepare = function(session, params, viewport)
        local argv
        if want_sixel(params, viewport) then
            -- Without pixels (the mode forced) the cell is taken for 8x16,
            -- the guess vterm makes too.
            local cell_w = viewport.pixel_width and viewport.pixel_width // viewport.columns or 8
            local cell_h = viewport.pixel_height and viewport.pixel_height // viewport.lines or 16
            argv = {
                "sh", "-c", render_script, "lua-sixel",
                session.local_path, "sixel",
                tostring(viewport.columns), tostring(viewport.lines),
                tostring(cell_w), tostring(cell_h),
            }
        else
            argv = {
                "sh", "-c", render_script, "lua-sixel",
                session.local_path, "symbols",
                tostring(viewport.columns), tostring(viewport.lines), "0", "0",
                "--colors=256",
            }
        end
        return {
            source = mc.source.process {
                argv = argv,
                stderr = "separate",
            },
            title = session.display_name,
            raw_path = session.local_path,
            initial_display = "terminal",
            auto_scroll = "top",
        }
    end,

    options = function(_, params)
        local dialog, dialog_error = mc.ui.dialog {
            title = "Image viewer",
            controls = {
                { id = "mode", type = "select", label = "Draw with:", value = params.mode,
                  options = {
                      { id = "auto", label = "Sixel where the terminal draws it" },
                      { id = "sixel", label = "Sixel" },
                      { id = "symbols", label = "Characters (chafa symbols)" },
                  } },
                { type = "hbox", expand_x = true, controls = {
                    { type = "spacer", expand_x = true },
                    { id = "ok", type = "button", label = "&OK", default = true },
                    { id = "cancel", type = "button", label = "&Cancel", cancel = true },
                } },
            },
        }
        if dialog == nil then
            if dialog_error ~= "cancelled" then
                mc.ui.message("Image viewer", dialog_error)
            end
            return nil
        end
        return { mode = dialog.values.mode }
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
