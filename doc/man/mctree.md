---
date: September 2026
---

# NAME

mctree - Structured view of a file in M-Commander.

# SYNOPSIS

**mctree**
[-bcdfstVx?] file

# DESCRIPTION

mctree is a link to
**mcommander**,
the main M-Commander executable.  Under this name the program opens the
internal viewer on the
*file*
given on the command line and puts it in structured mode at once, which is
what
**F3**
followed by the display mode key does inside the program.

In structured mode the viewer reads the file as a tree instead of a stream of
lines: a directory tree for an archive, a document outline for markup, a list
of sections for a program.  What a file turns into is decided by the display
modes the viewer has for its type.

The name needs a file.  Without one the program says so and stops.

# OPTIONS

The options are those of
**mcommander**,
and only the ones that concern the viewer have an effect.  See
**mcommander**(1)
for the full list.

# FILES

*{{pkgdatadir}}/help/mcommander.md*
: The help file for the program.

*~/.config/mc6/ini*
: User's own setup, the viewer options among it.

# LICENSE

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation.  See the built-in
help of M-Commander for details on the License and the lack
of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

mcommander(1), mview(1), mcstruct(1)

# BUGS

Bugs should be reported to
<https://github.com/blue-panels/mcommander/issues> .
