# SFTP Plugin <!-- help:notitle -->

**SFTP panel plugin**

Browses the files of another machine over SSH in a panel. The connection is
made by libssh2, so no ssh command and no mounted file system are needed.

**Opening it**

Alt-F1 or Alt-F2, then SFTP, opens the list of saved connections
(*sftp:/*).
Entering a connection opens it at its remote path.

**The list of connections**

**Enter**
: Connect and open the remote path.

**F4**
: Edit the connection under the cursor.

**Shift-F4**
: Create a connection.

**F5, F6**
: Copy the connection under the cursor into a new one.

**F8**
: Delete the connection.

**The connection dialog**

*Connection name*
: What the list shows.

*Host*, *Port*
: The machine and its port, 22 unless something else is given.

*User*
: The login name. Empty means the name of the local user.

*Password*
: Used when no key answers. It is kept in
*sftp.ini*
as it is typed.

*Key file*
: The private key to authenticate with. An agent is used first where one is
running, then the key file, then the password.

*Remote path*
: Where the panel opens after connecting.

**In a directory**

**Ctrl-r**
: Read the directory again, past the cache.

**F3, F4**
: View and edit a file; it is fetched into a temporary one and written back on
save.

**F5, F6**
: Copy and move between the machine and the other panel, in both directions.

**F7, F8**
: Create a directory, delete what is marked.

**Settings**

The connections and the hotkeys are kept in
*~/.config/mc6/sftp.ini*,
which the settings of the plugin open from the Manage plugins dialog.
