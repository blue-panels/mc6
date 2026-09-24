---
date: September 2026
---

# NAME

mview - Internal file viewer of M-Commander.

# SYNOPSIS

**mview**
[-bcCdfhstVx?] file

# DESCRIPTION

mview is a link to
**mcommander**,
the main M-Commander executable.  Executing
M-Commander under this name requests staring the internal viewer and
opening the
*file*
specified on the command line.

# OPTIONS

*-b*
: Force black and white display.

*-c*
: Force color mode on terminals where
**mview**
defaults to black and white.

*-d*
: Disable mouse support.

*-f*
: Display the compiled-in search paths for M-Commander files.

*-S arg, --skin=arg*
: Specify a name of skin in the command line.  See the
**Skins**
section in mcommander(1) for more information.

*-t*
: Used only if the code was compiled with S-Lang and terminfo: it makes
M-Commander use the value of the
**TERMCAP**
variable for the terminal information instead of the information on
the system wide terminal database

*-V*
: Displays the version of the program.

*-x*
: Forces xterm mode.  Used when running on xterm-capable terminals (two
screen modes, and able to send mouse escape sequences).

# FILES

*{{pkgdatadir}}/help/mcommander.md*
: The help file for the program.

*{{pkgdatadir}}/mc.ini*
: The default system-wide setup for M-Commander, used only if
the user's own ~/.config/mc6/ini file is missing.

*{{pkgdatadir}}/defaults.ini*
: Global settings for M-Commander. Settings in this file
affect all users, whether they have ~/.config/mc6/ini or not.

*~/.config/mc6/ini*
: User's own setup.  If this file is present, the setup is loaded from
here instead of the system-wide startup file.

# LICENSE

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation.  See the built-in
help of M-Commander for details on the License and the lack
of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

mcommander(1), mcedit6(1)

# BUGS

Bugs should be reported to
<https://github.com/blue-panels/mcommander/issues> .
