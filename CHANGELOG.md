# Changelog

The releases of this fork, newest first.

## 6.0.2 - 2026-07-31

- Copying, overwriting and resuming now work between plugin panels.
- The HTML provider has been removed.
- Warnings fixed by using long for editor column instead of off_t
- Commands run before the first Ctrl-O no longer lose their console output.
- Large pastes no longer freeze the UI; screen repaints are limited while input is arriving.
- The arcmc 'Create archive' hotkey is now reassignable via arcmc.ini.
- The legacy built-in tarfs and cpiofs VFS modules have been removed; arcmc handles archive browsing when available.
