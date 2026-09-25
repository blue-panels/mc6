# FTP Plugin <!-- help:notitle -->

**FTP panel plugin**

Browses an FTP server in a panel. Files are listed, read, written and deleted
like ordinary ones; what the server does not allow is refused by the server,
not by the plugin.

**Opening it**

Alt-F1 or Alt-F2, then FTP, opens the list of saved connections
(*ftp:/*).
The panel starts at that list; entering a connection opens it and shows the
remote path of the connection.

**The list of connections**

**Enter**
: Connect and open the remote path of the connection.

**F4**
: Edit the connection under the cursor. The key is settable as
*hotkey_edit*
of
*ftp.ini*.

**Shift-F4**
: Create a connection.

**F5, F6**
: Copy the connection under the cursor into a new one, to change one field of
it.

**F8**
: Delete the connection.

**The connection dialog**

Three pages, which the buttons at the top switch between.

*Connection name*
: What the list shows. Any text.

*Host*, *Port*
: The server and its port, 21 unless something else is given.

*User*, *Password*
: The login. An empty user means anonymous. The password is kept in
*ftp.ini*
and is stored as it is typed, so a shared machine is a bad place for it.

*Remote path*
: Where the panel opens after connecting.

The Advanced page holds the passive mode, the timeouts and the character set
the names of the server are read in; the defaults suit most servers.

**In a directory**

**Ctrl-r**
: Read the directory again, past the cache. The key is settable as
*hotkey_refresh*.

**F3, F4**
: View and edit a file. The file is fetched into a temporary one first, and
written back when the editor saves it.

**F5, F6**
: Copy and move between the panel of the server and the other panel, in both
directions.

**F7, F8**
: Create a directory, delete what is marked.

**Settings**

The plugin keeps its settings in
*~/.config/mc6/ftp.ini*:
the connections, one section each, and the hotkeys. The Manage plugins dialog
of the Options menu opens the same file through the settings of the plugin.
