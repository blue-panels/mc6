# Changelog

The releases of this fork, newest first.

## 6.0.3 - 2026-08-02

- The built-in tar and cpio filesystems are gone. Archives are opened by the
  arcmc panel plugin, which comes in the mc6-plugins package.
- The Fedora package could not be installed: it asked for interpreter paths
  that no package owns. Packages are now installed in a clean image before a
  release is published.
- Spell checking finds aspell on a system that has only the library, without
  the development package.
- The embedded terminal answers the queries a shell sends, so fish no longer
  waits ten seconds and leaves the query on the screen.
- mc starts when the login shell is a plain sh, as it is on FreeBSD.
- Non-ASCII characters typed in the editor appear at once, not after the next
  keypress.
- The spell engine can be switched between aspell and hunspell in the plugin
  settings.
- Panel plugins can show a Quick View preview in the other panel.
- Spell checking is built in and needs no aspell headers to compile.
- The archive plugin is built where libarchive ships no pkg-config file.

## 6.0.2 - 2026-07-31

- Copying, overwriting and resuming now work between plugin panels.
- The HTML provider has been removed.
- Warnings fixed by using long for editor column instead of off_t
- Commands run before the first Ctrl-O no longer lose their console output.
- Large pastes no longer freeze the UI; screen repaints are limited while input is arriving.
- The arcmc 'Create archive' hotkey is now reassignable via arcmc.ini.
- The legacy built-in tarfs and cpiofs VFS modules have been removed; arcmc handles archive browsing when available.
