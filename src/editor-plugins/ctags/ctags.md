# Ctags <!-- help:notitle -->

**Ctags plugin of the editor**

Jumps to the definition of the name under the cursor, and finds where a name
is used, from a tags file that ctags built.

**The tags file**

The plugin looks for
*tags*
in the directory of the file and in the directories above it, so a tags file
at the top of a project serves the whole tree. It is built by ctags, for
example

```
ctags -R .
```

Without a tags file the commands say so and do nothing.

**The keys**

**Alt-Enter**
: Jump to the definition of the name under the cursor. Where several
definitions answer, a list asks which one.

**Alt-Minus, Alt-Plus**
: Back to where the last jump started, and forward again, like the back and
forward of a browser.

**The dialogs**

The list of definitions shows the file, the line and the kind of every hit;
Enter opens the one under the cursor, Esc leaves the editor where it is. The
list of references does the same for the places a name is used.

**Settings**

The settings of the plugin, which the Manage plugins dialog of the Options
menu opens, hold the name of the tags file, whether to search the directories
above, and the keys the plugin answers to. They are kept in
*~/.config/mc6/ctags.ini*,
and the keys in
*ctags.keymap*
of the same directory.
