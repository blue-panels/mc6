---
date: September 2026
---

# NAME

mcstruct - Show a binary file as a tree of named fields.

# SYNOPSIS

**mcstruct**
*file*
*[definition]*

# DESCRIPTION

mcstruct is a link to
**mcommander**,
the main M-Commander executable.  Under this name the program opens the
*file*
in the structure panel: the bytes are read through a definition and shown as a
tree of named fields, with the value of each field beside its name and the
place it was read from.

Without a second argument the definition is chosen by the name and the
contents of the file.  Give
*definition*
to name one instead: a bare name is looked for in the directories of
definitions, while a name with a slash in it is a path, taken from the
current directory when it is relative.

The name and the first bytes are matched against the table in
*stl.als*
of each directory; where nothing matches, a definition named after the
extension of the file is tried, and
*stl.def*
last of all.

The definitions are looked for in
*~/.config/mc6/mcstruct*
first, then in
*{{sysconfdir}}/mcommander/mcstruct*,
and last among the ones that come with the package, so a definition of your
own hides the one of the same name below it.

The name needs a file.  Without one the program says so and stops.

# OPTIONS

The options are those of
**mcommander**.  See
**mcommander**(1)
for the full list.

# FILES

*{{panel_plugins_dir}}/mcstruct/data*
: The definitions that come with the package, one file per format.

*{{panel_plugins_dir}}/mcstruct/mcstruct_panel.md*
: The help text of the panel, which
**F1**
opens inside it.

*~/.config/mc6/mcstruct/*, *{{sysconfdir}}/mcommander/mcstruct/*
: Definitions of your own, read before the ones of the package.

# LICENSE

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation.  See the built-in
help of M-Commander for details on the License and the lack
of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

mcommander(1), mview(1), mctree(1)

# BUGS

Bugs should be reported to
<https://github.com/blue-panels/mcommander/issues> .
