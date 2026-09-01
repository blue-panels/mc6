# Changelog

The releases of this fork, newest first.

## 6.0.4 - 2026-09-01

- In line drawing mode the editor widens a table: Space on a frame character
  inserts a cell in the same column of every row, and Enter after the last
  frame character adds a row.
- Lua packages can read and change the editor's typing mode through
  editor:overwrite() and editor:set_overwrite().
- The terminal draws the DEC line drawing set an application switches to, so
  the frames of programs such as tig come out as lines and not as letters.
- Ctrl+Alt+L, or Ctrl+Shift+L where the terminal tells the two apart, clears
  the terminal screen and its scrollback and leaves the prompt where it is.
- Alt+Shift+S puts a line filter on at once, with the pattern taken from a one
  line selection or from the word under the cursor; it works in the editor and
  in the terminal, and Alt-S lifts it.
- The editor's search dialog opens with the selected text as the search
  string, still editable before the search starts.
- The terminal wraps long lines and reflows the scrollback when the width
  changes, so the tail of a long line is no longer lost.
- lua-dbf opens dBase, FoxPro and Clipper tables as pageable tables, with the
  structure, the record card, a choice of encoding and F8 for the raw file.
- mc.ui.screen lets a Lua script build a full screen of native widgets -
  tables, text, inputs, checkboxes - and drive it from events.
- The editor's Search dialog gains a Filter action that hides every line that
  does not match, keeping the original line numbers; Alt-S lifts the filter or
  puts the last one back on.
- mcstruct shows a binary file as a tree of named fields described by STL5
  definitions, with a hex strip, editing in place and definitions for many
  common formats.
- The new permission_colors option colours each permission bit by what it is,
  the way eza does, while the file type keeps the row colour.
- The image viewers show the picture first with one line about it; i switches
  to the full properties and F1 opens the handler's own help.
- Configuration, data and cache move to ~/.config/mc6, ~/.local/share/mc6 and
  ~/.cache/mc6; on first start the old mc directories are copied and the
  originals are left untouched.
- Sixel pictures work in the terminal and in the viewer, and the lua-sixel
  handler draws images with Chafa where the terminal supports it.
- lua-chafa shows a short description of an image before the picture and stops
  at one frame of an animated image.
- Terminal output can be marked with the mouse or with Shift and the arrows
  and copied with Ctrl-Insert; Ctrl-L scrolls the visible screen into the
  history.
- Alt-C asks for the directory in the panel's mini status instead of a dialog,
  with directory completion above the input and history on Up and Down.
- At the shell prompt the shell's own line editor owns the command line, so
  editing, history and completion work as they do in a terminal.
- A Lua handler runs readelf and shows the headers, sections, segments and
  dynamic data of an ELF file; F8 switches to the raw file.
- Shift-Tab unindents a block in the editor even when the ctags plugin holds
  the key for CtagsComplete.
- A generated view keeps its source while the viewer is open, and F8 switches
  between the generated data and the file itself.
- Alt+Shift+S filters the panel as the pattern is typed; Quick Search and
  Quick Filter share the pattern, and Ctrl-G removes the filter.
- A Lua handler can take over opening a file in the viewer or in Quick View;
  the Chafa handler uses this to show images.
- While a command runs, the prompt shows that command instead of the word
  background.
- The build asks for GLib 2.58, the version the code already needs.
- Lua scripting: a typed host ABI opens the editor, the panels, the viewer,
  the UI, processes and events to scripts, which are managed from Options.
- The embedded terminal replaces the old subshell: commands go to the shell
  already running there, and the command line shows that shell's real prompt.
- Terminal output can be selected and copied out, the terminal has a keymap
  and a skin section of its own, and the focus moves between it and the
  command line.
- The list of found files shows time, size and permissions, and groups the
  matches of a file under an entry that opens to the lines themselves.
- The terminal keeps a bounded scrollback: PgUp/PgDn, Shift and the arrows,
  Home/End and the wheel browse it, and typing returns to the live output.
- A panel plugin can hand another plugin an open seekable stream, and
  magic.ini routes a file to a plugin, so an archive on a remote host opens
  without downloading it whole.
- A read only SQLite browser opens a database as a panel of tables, columns
  and rows.
- Shift-F4 makes a new connection from the top listing of a plugin panel.
- The drive menu of each panel is in the Panel menu, with its shortcut.
- In the editor a space widens a table only while Insert is on; in overwrite
  mode it rubs the frame character out, as before.
- The gap before a column block keeps the editor's background, and copying a
  column block no longer loses bytes of multibyte characters.
- In an 8-bit locale the characters of the codepage are drawn as letters and
  not as dots, so CP866 Cyrillic comes out right in the editor and in the
  viewer.
- The mongo plugin builds with mongo-c-driver 1.24 again.
- The sqlite plugin no longer leaks the base name every time the title is
  rebuilt.
- Nothing zsh draws is lost any more: the terminal keeps the search prompt,
  the completion list and the tail of a long line.
- Shift-F3 diffs a file inside a commit and from the status list of the git
  panel.
- Switching between the panels and the terminal repaints the screen, so the
  panels no longer come back with holes in them.
- Ctrl+F1 and Ctrl+F2 toggle the panels while the focus is in the command
  line.
- A command run from the panels brings them back the way "Pause after run"
  asks, and the shell follows the current panel.
- The teaching examples are no longer installed among the loaded scripts, so
  the editor does not run them on every save.
- The terminal handles ICH and insert mode, so typing in the middle of a shell
  command line no longer overwrites what is there.
- Enter after Ctrl-O works on a directory again, and F9 opens the menu with
  the panels hidden.
- arcmc reads external archive formats from arcmc.ini, so a new format needs
  no rebuild, and Ctrl+PgDn opens such an archive.
- The active panel stays the active one when the terminal overlay opens.
- Entering a subdirectory of a plugin panel lands on "..", not on the first
  entry.
- sftp and shell-link ask for libssh2 1.9 and say why a connection failed.
- The build works with GLib 2.58 again, the version configure asks for.
- shell-link shows the owner and the group of remote files.
- Characters of an 8-bit codepage are drawn correctly on an 8-bit terminal.
- In a UTF-8 terminal the characters converted from CP1251 or KOI8-R are no
  longer shown as dots.
- The cursor and the state of the command line stay in step with the shell.
- Copying a directory out of a plugin panel copies what is in it.
- arcmc copies the contents of a directory taken out of an archive.
- The Esc key modes work as they are configured, and input no longer shows one
  key late.
- The mongo plugin is packaged for Ubuntu 26.04.
- arcmc opens password protected 7z archives.
- The Serbian man pages follow the build options, so the installed pages match
  the features built in.
- A long path in an extension cd no longer overflows the buffer.
- Full screen mcterm has a function key bar of its own, and the manual says
  what the scrollback is and which keys go to mc and which to the program in
  the terminal.
- The editor line filter feeds lines to the search engine through the normal
  buffer callback, shows progress and can be cancelled.

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
