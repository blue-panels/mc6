# main <!-- help:notitle -->

```
 ┌─┬─┐                                  - -        _
 │ │ │─┌.┌┐┌┬┐┌┬┐┌┐┌┐┌┤┌┐┌.             .'')}_____/
 │   │ └.└┘      └└  └└└─┴               ~_/      )
 ┴   ┴  {{MC_VERSION:32}}                (_(_/-(_/
```
 based on GNU Midnight Commander

This is the main help screen for **M-Commander**.

To learn more on how to use the interactive help facility just press [Enter](#how-to-use-help).  You may want to go directly to the help [contents](#contents).

GNU Midnight Commander is written by its [authors](#authors).

M-Commander comes with ABSOLUTELY NO [WARRANTY](gnu-gpl-v3.0.md#warranty). This is free software, and you are welcome to redistribute it under terms of [GNU General Public License](gnu-gpl-v3.0.md#license).

# Query boxes <a id="querybox"></a>

In the query dialog box you can use the arrow keys or the first letter to select an item or click with the mouse on the button.

# How to use help <a id="how-to-use-help"></a>

You can use the cursor keys or mouse to navigate in the help viewer.  Press **down arrow** to move to the next item or scroll down.  Press **up arrow** to move to the previous item or scroll up.  Press **right arrow** to follow the current link.  Press **left arrow** to go back in the history of nodes that you have visited.

If you terminal doesn't support the cursor keys you can use the **space bar** to scroll forward and the **b** (back) key scroll back.  Use the **TAB** key to move to the next item and press **ENTER** to follow the current link.  The **l** (last) key can be used to go back in the history of nodes you have visited.  Press **ESC** to exit the help viewer.

The left mouse button will follow the link or scroll.  The right mouse button can be used to go back in the history of nodes.

The full key list of the help viewer:

[General movement keys](#general-movement-keys) are accepted.

**tab**           Move to the next item.
**M-tab**         Move to the previous item.
**down**          Move to the next item or scroll a line down.
**up**            Move to the previous item or scroll a line up.
**right**, **enter**  Follow the current link.
**left**, **l**       Go back in the history of visited nodes.
**F1**            Show the help for the help viewer.
**n**             Go to the next node.
**p**             Go to the previous node.
**c**             Go to the Contents node.
**F10**, **esc**      Exit the help viewer.

# Key Bindings <!-- help:notitle --><a id="key-bindings"></a>

**Key Bindings**

View and change keyboard shortcuts for mc actions.

**Keys**

**Enter**
: Replace the shortcut: press the key to assign.

**F5**
: Add an extra shortcut for this action.

**F8, Del**
: Remove the shortcut.

**Save**
: Write the changes to
*~/.config/mc6/keymap.ini*.

**Edit keymap file**
: Open
*keymap.ini*
in the editor.

**Edit term file**
: Open the terminal key definitions.

Actions marked with \* differ from defaults.

# Learn keys <!-- help:notitle --><a id="learn-keys"></a>

**Learn Terminal Keys**

Teach mc the escape sequences your terminal sends
for function keys, arrows and navigation keys.

**Usage**

```
1. Check the modifier (Ctrl, Alt, Shift)
2. Click a key button -- press that key
3. Wait until the capture message disappears
4. Save when done
```

```
Del              Clear a learned key
Save             Write to ~/.config/mc6/term/<TERM>
Edit term file   Open the file in editor
```

# Key Sniffer <!-- help:notitle --><a id="key-sniffer"></a>

**Key Sniffer**

Press Capture, then any key. Shows:

```
Shortcut   Symbolic name (e.g. Ctrl-F5)
Action     Bound action in current keymap
Raw        Escape sequence and hex bytes
Keycode    Internal numeric code
```

Useful for diagnosing terminal key problems.
