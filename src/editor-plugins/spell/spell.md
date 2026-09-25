# Spell <!-- help:notitle -->

**Spell checking**

Checks the text of the editor with aspell and offers what it suggests. The
aspell package and the dictionary of the language have to be installed.

**The keys**

**Ctrl-p**
: Check the word under the cursor. The word is replaced by what is chosen, or
left as it is.

The whole file is checked from the Plugins menu of the editor, which walks the
words one by one and stops at every one aspell does not know.

**The dialog**

The word aspell stumbled on is shown with what it suggests.

**Enter**
: Take the suggestion under the cursor and go on.

**Esc**
: Leave the word as it is and stop the check.

The buttons of the dialog skip the word once, add it to the personal
dictionary so that it is never asked about again, or change the language of
the check.

**Settings**

The language is the
*spell_language*
setting of the
**[Misc]**
section of
*~/.config/mc6/ini*:
a name that aspell knows, en, ru and so on, which

```
aspell --list languages
```

prints. The value
**NONE**
switches the checking off. The keys of the plugin are in
*spell.keymap*
of the same directory.
