# arcmc archive browser

`arcmc` is a panel plugin for browsing, creating and extracting archives. It
uses libarchive where possible and can use command-line archivers for formats
that libarchive cannot read or write.

## Configuration files

The plugin stores its settings in `arcmc.ini` in the Midnight Commander user
configuration directory, normally:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/mc6/arcmc.ini
```

The settings dialog writes this file. If it is edited while Midnight Commander
is running, restart Midnight Commander before testing the changes.

File-operation associations are configured separately in `magic.ini` in the
same directory. `arcmc.ini` tells the plugin how to handle an archive format;
`magic.ini` tells the file manager which action should invoke the plugin for a
matching file.

## Adding an external archive format

A format that is not compiled into `arcmc` can be registered at runtime. For
example:

```ini
[arcmc-ext]
FOO=true

[arcmc-ext-params-FOO]
extension=.foo
pack_bin=foo-archive
pack_args=create
unpack_bin=foo-archive
unpack_args=extract
test_bin=foo-archive
test_args=test
extfs_helper=ufoo
list_file_arg=@%s
```

`FOO` is an arbitrary, case-insensitive format name. A previously unknown name
must have an `extension` entry. The leading dot is optional and is added when
the configuration is loaded. Extension matching is case-insensitive; when
suffixes overlap, the longest matching suffix wins.

Entries under `[arcmc-ext]` enable or disable formats. A missing entry defaults
to `true`.

The keys in an `[arcmc-ext-params-NAME]` section are:

| Key | Purpose |
| --- | --- |
| `extension` | Filename suffix used to recognize the format. `ext` is accepted as a compatibility alias. |
| `pack_bin` | Program used to create or update archives. |
| `pack_args` | Arguments placed after `pack_bin` and before the archive name. |
| `unpack_bin` | Program used to extract archives. |
| `unpack_args` | Arguments placed after `unpack_bin` and before the archive name. |
| `test_bin` | Program used by the archive test action. |
| `test_args` | Arguments placed after `test_bin` and before the archive name. |
| `extfs_helper` | Executable extfs helper used to list an archive and extract individual entries. |
| `list_file_arg` | `printf`-style template for a file-list argument, for example `@%s`. It is used when a pack or selective-extract command would exceed 128 KiB. |

Missing command keys make the corresponding operation unavailable. An empty
value clears a default value. The `*_args` values are shell command fragments,
not argument arrays or placeholder templates.

The resulting external commands have these forms:

```text
pack_bin pack_args archive files...
unpack_bin unpack_args archive [destination]
test_bin test_args archive
```

For selective extraction, `arcmc` changes to the destination directory and
then runs `unpack_bin unpack_args archive files...`.

## How extfs helpers are used

The scripts in [`src/vfs/extfs/helpers/`](../../vfs/extfs/helpers/) implement
the extfs command protocol. `arcmc` reuses that protocol as an adapter for
external archive tools; it does not open an extfs VFS path.

For `extfs_helper=ufoo`, `arcmc` searches for an executable named `ufoo` in
this order:

1. The user's MC data directory, normally
   `${XDG_DATA_HOME:-$HOME/.local/share}/mc6/extfs.d/`.
2. The system `extfs.d` directory below MC's configured library-executable
   directory, for example `/usr/libexec/mc/extfs.d/`.

The helper commands used by `arcmc` are:

```text
ufoo list archive.foo
ufoo copyout archive.foo path/inside/archive destination
ufoo copyin archive.foo path/inside/archive source
ufoo rm archive.foo path/inside/archive
```

`list` is required for browsing a format that libarchive cannot read. Its
standard output must use the modified `ls -l` format described in the
[extfs helper documentation](../../vfs/extfs/helpers/README). `copyout` is
required to view or copy an individual file. `copyin` and `rm` are optional;
when implemented, they allow adding and deleting files from the archive panel.

The helper must be executable. A user helper can therefore be installed with:

```sh
install -Dm755 ufoo "${XDG_DATA_HOME:-$HOME/.local/share}/mc6/extfs.d/ufoo"
```

An external helper can only operate on an archive that has a local filesystem
path. It cannot browse an archive exposed only as a non-local plugin stream.

## Opening a custom format with Ctrl+PgDn

Registering an extension in `arcmc.ini` does not create a file-operation
association. To open `.foo` files with Ctrl+PgDn, add this rule to the user's
`magic.ini`:

```ini
[arcmc.foo]
Regex=\\.foo$
RegexIgnoreCase=true
CdChild=%plugin{arcmc:open}
```

`CdChild` is the Ctrl+PgDn action. It leaves the normal Enter action unchanged.
Use `Open=%plugin{arcmc:open}` only when Enter should open the archive too.

## Inno Setup installers

Inno Setup support is enabled by default and does not need to be copied into
`arcmc.ini`. Its built-in registry entry is equivalent to:

```ini
[arcmc-ext]
INO=true

[arcmc-ext-params-INO]
extension=.exe
test_bin=innoextract
test_args=--test --silent
extfs_helper=uinno
```

The installed `uinno` helper uses `innoextract` for `list` and `copyout`, so
`innoextract` must be available in `PATH`. The supplied `magic.ini` rule maps
`.exe` files to `arcmc` through Ctrl+PgDn, without changing Enter.
