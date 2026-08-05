# Midnight Commander with Plugins

`mc6` -- a fork of [GNU Midnight Commander](https://midnight-commander.org),
based on version 4.8.33.

Midnight Commander (MC) is a text-mode, full-screen file manager: two panels,
a built-in editor and viewer, and a virtual filesystem for browsing archives
and remote hosts. It runs on the OS console, in xterm, and over ssh.

This fork is not an official GNU package. Report issues here, not upstream.

Own version numbering starts at `v6.0.1`, independent of upstream.

![Git panel with inline diff](https://raw.githubusercontent.com/wiki/ilia-maslakov/mcdev/assets/git-panel.gif)

## What this fork adds

Full notes: **[Releases wiki](https://github.com/ilia-maslakov/mcdev/wiki/Releases)**.

- **Panel plugins.** Panel contents can come from a dynamically loaded plugin.
  Shipped: git, docker, Kubernetes, MongoDB, S3, FTP/FTPS, SFTP, Samba, systemd,
  shell connections, External Panelize, and arcmc. The old built-in `ftpfs` and
  `sftpfs` VFS modules are replaced by the FTP and SFTP plugins.
- **arcmc** — an archive manager on libarchive: browse, create, pack and extract
  (zip, 7z, tar.\*, cpio) with progress and cancel. The legacy built-in `tarfs`
  and `cpiofs` VFS modules have been removed.

  ![arcmc](https://raw.githubusercontent.com/wiki/ilia-maslakov/mcdev/assets/arcmc.gif)
- **Editor** — code folding, an undo history browser, a macro explorer, and an
  editor plugin framework.
- **Viewer** — a structured tree mode for JSON, YAML, XML and HTML, a grep-style
  live filter, ANSI colour and terminal replay, and streaming of never-ending
  command output.

  ![Structured tree viewer](https://raw.githubusercontent.com/wiki/ilia-maslakov/mcdev/assets/viewer-tree.gif)

  ![Grep-style live filter](https://raw.githubusercontent.com/wiki/ilia-maslakov/mcdev/assets/viewer-filter.gif)
- **Embedded terminal** — run a shell inside the file manager, panels stay in
  sync with its directory.
- **Panels** — user-editable view modes, dialogs for managing key bindings and
  learning terminal keys, and the classic hide-a-panel / run-a-command flow.

  ![Hide a panel, run a command](https://raw.githubusercontent.com/wiki/ilia-maslakov/mcdev/assets/panel-hide.gif)

## Building

See [`INSTALL`](INSTALL) for dependencies and instructions.

```sh
./autogen.sh          # from a git checkout
./configure
make
sudo make install
```

`mc --version` identifies this fork.

## Packages

This fork is packaged as **`mc6`**, while its commands deliberately remain
`mc`, `mcedit`, `mcview`, `mcdiff` and `mctree`.  It replaces the distribution
`mc` package rather than coexisting with it.

There is no public package repository yet.  Each release builds `.deb`, `.rpm`
and `.pkg.tar.zst` and attaches them to its GitHub release; to build them
yourself, see [`packaging/README.md`](packaging/README.md).  The `.deb` assets
are built separately for Debian Trixie and Ubuntu 26.04; install only the one
whose distribution suffix matches your system.  The recipes carry no version
of their own: `packaging/prepare.sh <version>` generates the version-bearing
files from the release tag, and every build starts with it.

- Debian Trixie: `sudo apt install ./mc6_*~debian13*.deb ./mc6-data_*~debian13*.deb ./mc6-plugins_*~debian13*.deb`
- Ubuntu 26.04: `sudo apt install ./mc6_*~ubuntu26*.deb ./mc6-data_*~ubuntu26*.deb ./mc6-plugins_*~ubuntu26*.deb`
- RHEL/Fedora: `sudo dnf swap mc mc6`, then `sudo dnf install ./mc6-plugins-*.rpm`
- Arch: `sudo pacman -U ./mc6-*.pkg.tar.zst ./mc6-plugins-*.pkg.tar.zst`
- Gentoo: copy `packaging/gentoo` into a local overlay as `app-misc/mc6`,
  then `emerge app-misc/mc6`

Install downloaded packages through the package manager -- `apt install
./file.deb` and `dnf install ./file.rpm` -- never `dpkg -i` or `rpm -i`, which
skip the dependency and replacement handling.

## Documentation

- [Wiki](https://github.com/ilia-maslakov/mcdev/wiki)
- Built-in help: press `F1` inside mc
- Manual pages: `mc(1)`, `mcedit(1)`, `mcview(1)`

## Reporting problems

Open an issue: <https://github.com/ilia-maslakov/mcdev/issues>

Include `mc --version`, your OS and distribution, and the compiler and
configure flags if you know them. For a crash, attach a `gdb` backtrace
(`gdb mc core`, then `where`).

## License

GNU General Public License, version 3 or any later version. See
[`COPYING`](COPYING).
