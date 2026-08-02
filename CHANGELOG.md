# Changelog

The releases of this fork, newest first.

## 6.0.3 - 2026-08-02

- You can now choose the spell engine (aspell or hunspell) in the spell plugin settings.
- Panel plugins can now provide Quick View previews shown in the neighbouring panel, like files or command outputs.
- Embedded terminals now respond to shell queries, preventing the ten-second wait and displayed hex query text.
- mc no longer hangs when the login shell is plain sh; it starts without a subshell, preserving the command line.
- Spell checking is built in unconditionally; aspell headers and --enable-aspell are no longer required at build time.
- The build now enables the arcmc plugin when libarchive is detected without a pkg-config file.
- Non-ASCII characters now appear immediately in the editor instead of showing up only after the next keypress.
- Spell checking now locates libaspell by trying a range of sonames, avoiding missing-library errors.
- Removed the cpio VFS and replaced it with the arcmc backend.
- The tar virtual filesystem was removed; open tar archives with external tools or other VFS backends.

## 6.0.2 - 2026-07-31

- Copying, overwriting and resuming now work between plugin panels.
- The HTML provider has been removed.
- Warnings fixed by using long for editor column instead of off_t
- Commands run before the first Ctrl-O no longer lose their console output.
- Large pastes no longer freeze the UI; screen repaints are limited while input is arriving.
- The arcmc 'Create archive' hotkey is now reassignable via arcmc.ini.
- The legacy built-in tarfs and cpiofs VFS modules have been removed; arcmc handles archive browsing when available.
