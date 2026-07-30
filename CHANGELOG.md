# Changelog

The releases of this fork, newest first.

## 6.0.2 - 2026-07-30

- The shell-link panel plugin now handles FISH connections directly; the core no longer links libssh2.
- XML files in mcview are now parsed by a built-in mctree parser; libxml2 and HTML provider are removed.
- Build warnings about column type mismatch are fixed; nothing changes in the editor for users.
- mcterm now preserves output from the first command and supports zsh.
- Pasting large text into the editor no longer freezes the UI; screen updates are now capped during paste.
- The arcmc "Create archive" hotkey is now user-configurable; all panel plugins handle hotkey config identically.

