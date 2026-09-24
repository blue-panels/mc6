---
date: September 2026
---

# NAME

mcedit6 - Internal file editor of M-Commander.

# SYNOPSIS

**mcedit6**
[-bcCdfhstVx?] [+lineno] [file1] [file2] ...

**mcedit6**
[-bcCdfhstVx?] file1:lineno[:] file2:lineno[:] ...

# DESCRIPTION

mcedit6 is a link to
**mcommander**,
the main M-Commander executable. Executing M-Commander
under this name runs the internal editor and opens files
specified on the command line. The editor is based on the terminal version of
**cooledit**
\- standalone editor for X Window System.

# OPTIONS

*+lineno*
: Go to the line specified by number (do not put a space between the
*+*
sign and the number). Several line numbers are allowed but only the last one
will be used, and it will be applied to the first file only.

*-b*
: Force black and white display.

*-c*
: Force ANSI color mode on terminals that don't seem to have color
support.

*-d*
: Disable mouse support.

*-f*
: Display the compiled-in search path for M-Commander data
files.

*-S arg, --skin=arg*
: Specify a name of skin in the command line.  See the
**Skins**
section in mcommander(1) for more information.

*-t*
: Force using termcap database instead of terminfo.  This option is only
applicable if M-Commander was compiled with S-Lang library
with terminfo support.

*-V*
: Display the version of the program.

*-x*
: Force xterm mode.  Used when running on xterm-capable terminals (two
screen modes, and able to send mouse escape sequences).

# FEATURES

The internal file editor is a full-featured windowed editor.  It can
edit several files at the same time. Maximum size of each file is 64
megabytes. It is possible to edit binary files. The features it presently
supports are: block copy, move, delete, cut, paste; key for key undo;
pull-down menus; file insertion; macro commands; regular expression
search and replace; shift-arrow text highlighting (if supported by
the terminal); insert-overwrite toggle; autoindent; tunable tab size;
syntax highlighting for various file types; and an option to pipe text
blocks through shell commands like indent and ispell.

Each file is opened in its own window in full-screen mode. Window control
in mcedit6 is similar to the window control in other multi-window program:
double click on window title maximizes the window to full-screen or restores
window size and position; left-click on window title and mouse drag moves
the window in editor area; left-click on low-right frame corner and mouse drag
resizes the window. These actions can be made using "Window" menu.

# KEYS

The editor is easy to use and can be used without learning.  The
pull-down menu is invoked by pressing F9.  You can learn other keys from
the menu and from the button bar labels.

In addition to that, Shift combined with arrows does text highlighting
(if supported by the terminal):
**Ctrl-Ins**
copies to the file
**~/.cache/mc6/mcedit6/mcedit6.clip**,
**Shift-Ins**
pastes from
**~/.cache/mc6/mcedit6/mcedit6.clip**,
**Shift-Del**
cuts to
**~/.cache/mc6/mcedit6/mcedit6.clip**,
and
**Ctrl-Del**
deletes highlighted text.  Mouse highlighting also works on some
terminals.  To use the standard mouse support provided by your terminal,
hold the Shift key.  Please note that the mouse support in the terminal
doesn't share the clipboard with
**mcedit6**.

The completion key (usually
**Meta-Tab**
or
**Escape Tab**)
completes the word under the cursor using the words used in the file.

The
**Filter**
button of the search dialog
(**F7**)
hides every line that does not match the search string, with the search
dialog's own type, case and whole-word settings.  Line numbers keep their
original values.  The set of hidden lines is fixed when the button is
pressed: editing never re-applies the match, so split, joined and newly
typed lines stay shown.
**Meta-s**
lifts the filter and, pressed again, puts the last search back on as a
filter.

# MACRO

To define a macro, press
**Ctrl-R**
and then type out the keys you want to be executed.  Press
**Ctrl-R**
again when finished.  The macro can be assigned to any key by pressing that key.
The macro is executed when you press the assigned key.

The macro commands are stored in section
**[editor]**
it the file
**~/.local/share/mc6/macros**.

External scripts (filters) can be assigned into the any hotkey by edit
**macros**
like following:

```
[editor]
ctrl-W=ExecuteScript:25;
```

This means that ctrl-W hotkey initiates the
*ExecuteScript(25)*
action, then editor handler translates this into execution of
**~/.local/share/mc6/mcedit6/macros.d/macro.25.sh**
shell script.

External scripts are stored in
**~/.local/share/mc6/mcedit6/macros.d/**
directory and must be named as
**macro.XXXX.sh**
where
**XXXX**
is the number from 0 to 9999.
See
**Edit Menu File**
for more detail about format of the script.

Following macro definition and directives can be used:

*#silent*
: If this directive is set, then script starts without an interactive shell.

*%c*
: The cursor column position number.

*%i*
: The indent of blank space, equal the cursor column.

*%y*
: The syntax type of current file.

*%b*
: The block file name.

*%f*
: The current file name.

*%n*
: Only the current file name without extension.

*%x*
: The extension of current file name.

*%d*
: The current directory name.

*%F*
: The current file in the unselected panel.

*%D*
: The directory name of the unselected panel.

*%t*
: The currently tagged files.

*%T*
: The tagged files in the unselected panel.

*%v* and *%V*
: Similar to the
*%t*
and
*%T*
, but expands to full names of tagged files.

*%u* and *%U*
: Similar to the
*%t*
and
*%T*
macros, but in addition the files are untagged. You can use this macro
only once per menu file entry or extension file entry, because next time
there will be no tagged files.

*%s* and *%S*
: The selected files: The tagged files if there are any. Otherwise the
current file.

Feel free to edit this files, if you need.
Here is a sample external script:

```
l       comment selection
	TMPFILE=`mktemp ${MC_TMPDIR:-/tmp}/up.XXXXXX` || exit 1
	echo #if 0 > $TMPFILE
	cat %b >> $TMPFILE
	echo #endif >> $TMPFILE
	cat $TMPFILE > %b
	rm -f $TMPFILE
```

If some keys don't work, you can use
**Learn Keys**
in the
**Options**
menu.

# CODE NAVIGATION

**mcedit6**
can be used for navigation through code with tags files created by etags
or ctags commands. If there is no TAGS file code navigation will not work.
For example, in case of exuberant-ctags for C language command will be:

ctags -e --language-force=C -R ./

**Meta-Enter**
shows list box to select item under cursor (cursor should stand at the end
of the word).

**Meta-Minus**
where minus is symbol "-" goes to previous function in navigation list
(like browser's Back button).

**Meta-Equal**
where equal is symbol "=" goes to next function in navigation list
(like browser's Forward button).

# SYNTAX HIGHLIGHTING

**mcedit6**
supports syntax highlighting.  This means that keywords and contexts
(like C comments, string constants, etc) are highlighted in different
colors.  The following section explains the format of the file
**~/.local/share/mc6/syntax/Syntax**.
If this file is missing, system-wide
**{{pkgdatadir}}/syntax/Syntax**
is used.
The file
**~/.local/share/mc6/syntax/Syntax**
is rescanned on opening of every new editor file.  The file contains
rules for highlighting, each of which is given on a separate line, and
define which keywords will be highlighted with what color.

The file is divided into sections, each beginning with a line with the
**file**
command.  The sections are normally put into separate files using the
**include**
command.

The
**file**
command has three arguments.  The first argument is a regular expression
that is applied to the file name to determine if the following section
applies to the file.  The second argument is the description of the file
type.  It is used in
**cooledit**;
future versions of
**mcedit6**
may use it as well.  The third optional argument is a regular expression
to match the first line of text of the file.  The rules in the following
section apply if either the file name or the first line of text matches.

A section ends with the start of another section.  Each section is
divided into contexts, and each context contains rules.  A context is a
scope within the text that a particular set of rules belongs to.  For
instance, the text within a C style comment (i.e. between
**/\***
and
**\*/**)
has its own color.  This is a context, although it has no further rules
inside it because there is probably nothing that we want highlighted
within a C comment.

A trivial C programming section might look like this:

```
file .\*\\.c C\sProgram\sFile (#include|/\\\*)

wholechars abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_

# default colors
define  comment   brown
context default
  keyword  whole  if       yellow
  keyword  whole  else     yellow
  keyword  whole  for      yellow
  keyword  whole  while    yellow
  keyword  whole  do       yellow
  keyword  whole  switch   yellow
  keyword  whole  case     yellow
  keyword  whole  static   yellow
  keyword  whole  extern   yellow
  keyword         {        brightcyan
  keyword         }        brightcyan
  keyword         '*'      green

# C comments
context /\* \*/ comment

# C preprocessor directives
context linestart # \n red
  keyword  \\\n  brightred

# C string constants
context " " green
  keyword  %d    brightgreen
  keyword  %s    brightgreen
  keyword  %c    brightgreen
  keyword  \\"   brightgreen
```

Each context starts with a line of the form:

**context**
[**exclusive**]
[**whole**|**wholeright**|**wholeleft**]
[**linestart**]
*delim*
[**linestart**]
*delim*
*[foreground]*
*[background]*
*[attributes]*

The first context is an exception.  It must start with the command

**context default**
*[foreground]*
*[background]*
*[attributes]*

otherwise
**mcedit6**
will report an error.  The
**linestart**
option specifies that
*delim*
must start at the beginning of a line.  The
**whole**
option tells that
*delim*
must be a whole word.  To specify that a word must begin on the word
boundary only on the left side, you can use the
**wholeleft**
option, and similarly a word that must end on the word boundary is specified by
**wholeright**.

The set of characters that constitute a whole word can be changed at any
point in the file with the
**wholechars**
command.  The left and right set of characters can be set separately
with

**wholechars**
[**left**|**right**]
*characters*

The
**exclusive**
option causes the text between the delimiters to be highlighted, but not
the delimiters themselves.

Each rule is a line of the form:

**keyword**
[**whole**|**wholeright**|**wholeleft**]
[**linestart**]
*string foreground*
*[background]*
*[attributes]*

Context or keyword strings are interpreted, so that you can include tabs
and spaces with the sequences \\t and \\s.  Newlines and backslashes are
specified with \\n and \\\\ respectively.  Since whitespace is used as a
separator, it may not be used as is.  Also, \\\* must be used to specify
an asterisk.  The \* itself is a wildcard that matches any length of
characters.  For example,

```
  keyword         '*'      green
```

colors all C single character constants green.  You also could use

```
  keyword         "*"      green
```

to color string constants, but the matched string would not be allowed
to span across multiple newlines.  The wildcard may be used within
context delimiters as well, but you cannot have a wildcard as the last
or first character.

Important to note is the line

```
  keyword  \\\n  brightgreen
```

This line defines a keyword containing the backslash and newline
characters.  Since the keywords are matched before the context
delimiters, this keyword prevents the context from ending at the end of
the lines that end in a backslash, thus allowing C preprocessor
directive to continue across multiple lines.

The possible colors are: black, gray, red, brightred, green,
brightgreen, brown, yellow, blue, brightblue, magenta, brightmagenta,
cyan, brightcyan, lightgray and white. The special keyword "default" means
the terminal's default. Another special keyword "base" means mcommander's main
colors, it is useful as a placeholder if you want to specify attributes
without modifying the background color. When 256 colors are available,
they can be specified either as color16 to color255, or as rgb000 to rgb555
and gray0 to gray23.

If the syntax file is shared with
**cooledit**,
it is possible to specify different colors for
**mcedit6**
and
**cooledit**
by separating them with a slash, e.g.

```
keyword  #include  red/Orange
```

**mcedit6**
uses the color before the slash.  See cooledit(1) for supported
**cooledit**
colors.

Attributes can be any of bold, italic, underline, reverse and blink, appended by a
plus sign if more than one are desired.

Comments may be put on a separate line starting with the hash sign (#).

If you are describing case insensitive language you need to use
**caseinsensitive**
directive. It should be specified at the beginning of syntax file.

Because of the simplicity of the implementation, there are a few
intricacies that will not be dealt with correctly but these are a minor
irritation.  On the whole, a broad spectrum of quite complicated
situations are handled with these simple rules.  It is a good idea to
take a look at the syntax file to see some of the nifty tricks you can
do with a little imagination.  If you cannot get by with the rules I
have coded, and you think you have a rule that would be useful, please
email me with your request.  However, do not ask for regular expression
support, because this is flatly impossible.

The special
**line-local**
syntax mode is intended for plain text.  It resets at every newline and
screen boundary, and only scans the displayed text.  It supports
**number**
*max foreground*
for decimal numbers up to
*max*
digits,
**string**
*quote foreground*
for single- or double-quoted strings, and
**symbols**
*characters foreground*
for individually colored punctuation characters.

A useful hint is to work with as much as possible with the things you
can do rather than try to do things that this implementation cannot deal
with.  Also remember that the aim of syntax highlighting is to make
programming less prone to error, not to make code look pretty.

The syntax highlighting can be toggled using Ctrl-s shortcut.

# OPTIONS

Most options can be set from Options dialog box.  See the
**Options**
menu.  The following options are defined in
**~/.config/mc6/ini**
and have obvious counterparts in the dialog box.  You can modify them to
change the editor behavior, by editing the file.  Unless specified, a 1
sets the option to on, and a 0 sets it to off, as usual.

*use_internal_edit*
: This option is ignored when invoking
**mcedit6**.

*editor_tab_spacing*
: Interpret the tab character as being of this length.
Default is 8. You should avoid using
other than 8 since most other editors and text viewers
assume a tab spacing of 8. Use
**editor_fake_half_tabs**
to simulate a smaller tab spacing.

*editor_fill_tabs_with_spaces*
: Never insert a tab character. Rather insert spaces (ascii 32) to fill to the
desired tab size.

*editor_return_does_auto_indent*
: Pressing return will tab across to match the indentation
of the first line above that has text on it.

*editor_backspace_through_tabs*
: Make a single backspace delete all the space to the left
margin if there is no text between the cursor and the left
margin.

*editor_fake_half_tabs*
: This will emulate a half tab for those who want to program
with a tab spacing of 4, but do not want the tab size changed
from 8 (so that the code will be formatted the same when displayed
by other programs). When editing between text and the left
margin, moving and tabbing will be as though a tab space were
4, while actually using spaces and normal tabs for an optimal fill.
When editing anywhere else, a normal tab is inserted.

*editor_option_save_mode*
: Possible values 0, 1 and 2.  The save mode (see the options menu also)
allows you to change the method of saving a file.  Quick save (0) saves
the file immediately, truncating the disk file to zero length (i.e.
erasing it) and then writing the editor contents to the file.  This
method is fast, but dangerous, since a system error during a file save
will leave the file only partially written, possibly rendering the data
irretrievable.  When saving, the safe save (1) option enables creation
of a temporary file into which the file contents are first written.  In
the event of a problem, the original file is untouched.  When the
temporary file is successfully written, it is renamed to the name of the
original file, thus replacing it.  The safest method is create backups
(2): a backup file is created before any changes are made.  You
can specify your own backup file extension in the dialog.  Note that
saving twice will replace your backup as well as your original file.

*editor_word_wrap_line_length*
: Line length to wrap at. Default is 72.

*editor_backup_extension*
: Symbol to add to name of backup files. Default is "~".

*editor_line_state*
: Show state line of editor. Currently it shows current line number (in the future
it might show things like folding, breakpoints, etc.). M-n toggles this option.

*editor_visible_spaces*
: Toggle "show visible trailing spaces".  If editor_visible_spaces=1, they are shown
as '.'

*editor_visible_tabs*
: Toggle "show visible tabs".  If editor_visible_tabs=1, tabs are shown as '<---->'

*editor_show_control_chars*
: Show control characters in caret notation, for example '^M' or '^L'.
If editor_show_control_chars=0, each of them takes one blank cell instead.
The "Toggle control characters" item of the Command menu switches this option.

*editor_persistent_selections*
: Do not remove block selection after cursor movement.

*editor_drop_selection_on_copy*
: Reset selection after copy to clipboard.

*editor_cursor_beyond_eol*
: Allow moving cursor beyond the end of line.

*editor_cursor_after_inserted_block*
: Allow moving cursor after inserted block.

*editor_syntax_highlighting*
: enable syntax highlighting.

*editor_edit_confirm_save*
: Show confirmation dialog on save.

*editor_option_typewriter_wrap*
: to be described

*editor_option_auto_para_formatting*
: to be described

*editor_option_save_position*
: Save file position on exit.

*source_codepage*
: Symbol representation of codepage name for file (i.e. CP1251, ~ - default).

*editor_group_undo*
: Combine UNDO actions for several of the same type of action (inserting/overwriting,
deleting, navigating, typing)

*editor_wordcompletion_collect_entire_file*
: Search autocomplete candidates in entire file (1) or just from
beginning of file to cursor position (0).

*editor_wordcompletion_collect_all_files*
: Search autocomplete candidates from all loaded files (1, default), not only from
the currently edited one (0).

*spell_language*
: Spelling language (en, en-variant_0, ru, etc) installed with aspell
package (a full list can be obtained using 'aspell' utility).
Use
**spell_language = NONE**
to disable aspell support. Default value is 'en'. Option must be located
in the [Misc] section.

*editor_stop_format_chars*
: Set of characters to stop paragraph formatting. If one of those characters
is found in the beginning of line, that line and all following lines of paragraph
will be untouched. Default value is
"**-+\*\\,.;:&>**".

*editor_state_full_filename*
: Show full path name in the status line. If disabled (default), only base name of the
file is shown.

# FILES

*{{pkgdatadir}}/help/mcommander.md*
: The help file for the program.

*{{pkgdatadir}}/mc.ini*
: The default system-wide setup for M-Commander, used only if
the user's own ~/.config/mc6/ini file is missing.

*{{pkgdatadir}}/defaults.ini*
: Global settings for M-Commander. Settings in this file
affect all users, whether they have ~/.config/mc6/ini or not.

*{{pkgdatadir}}/syntax/\**
: The default system-wide syntax files for mcedit6, used only if
the corresponding user's own file in
**~/.local/share/mc6/syntax/**
is missing.

*~/.config/mc6/ini*
: User's own setup.  If this file is present then the setup is loaded
from here instead of the system-wide setup file.

*~/.local/share/mc6/mcedit6/*
: User's own directory where block commands are processed and saved and
user's own syntax files are located.

# LICENSE

This program is distributed under the terms of the GNU General Public
License as published by the Free Software Foundation.  See the built-in
help of M-Commander for details on the License and the lack
of warranty.

# AVAILABILITY

The latest version of this program can be found at
<https://github.com/blue-panels/mcommander/releases> .

# SEE ALSO

cooledit(1), mcommander(1), gpm(1), terminfo(1), scanf(3).

# AUTHORS

Paul Sheer (psheer@obsidian.co.za) is the original author of
M-Commander's internal editor.

# BUGS

Bugs should be reported to
<https://github.com/blue-panels/mcommander/issues> .
