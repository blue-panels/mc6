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
**Alt-s**
(or
**t**)
does inside the viewer.

In structured mode the viewer reads the file as a tree instead of a stream of
lines.  It reads JSON, YAML and XML, and an HTML file with the parser of XML;
the format is taken from the name of the file, from what the file command
says about it, and from the text itself.  The tree works on local files only.
A file it cannot parse, or one larger than the tree view takes, is reported
and stays plain text.  The keys of the tree are described in
**mview**(1).

The name needs a file.  Without one the program says so and stops.  Only the
first file on the command line is opened.

# OPTIONS

The options are those of
**mcommander**,
and only the ones that concern the viewer have an effect.  See
**mcommander**(1)
for the full list.

# FILES

*{{pkgdatadir}}/help/mview.md*
: The help file of the viewer, which F1 opens.

*{{pkgdatadir}}/help/mcommander.md*
: The help file of the file manager, where the rest of the help lives.

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
