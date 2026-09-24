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

Without a second argument the definition is chosen by the contents of the
file, from the ones that come with the package.  Give
*definition*
to name one instead; a relative path is taken from the current directory.

The name needs a file.  Without one the program says so and stops.

# FILES

*{{panel_plugins_dir}}/mcstruct/data*
: The definitions that come with the package, one file per format.

*{{panel_plugins_dir}}/mcstruct/mcstruct_panel.hlp*
: The help text of the panel, also reachable with
**F1**
inside it.

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
