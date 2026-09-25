# Shell Link Plugin <!-- help:notitle -->

**Shell link panel plugin**

Browses the files of another machine in a panel over an ssh connection. The
other side needs no server of its own: the plugin runs a small helper in the
shell it logged into, and talks to it.

**Opening it**

Alt-F1 or Alt-F2, then Shell link, opens the list of saved connections
(*shell:/*).
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
: The login name; empty means the name of the local user.

*Password*, *Key file*
: How to log in. An agent is used first where one is running, then the key
file, then the password.

*Remote path*
: Where the panel opens after connecting.

**In a directory**

**Ctrl-r**
: Read the directory again, past the cache.

**F3, F4**
: View and edit a file; it is fetched into a temporary one and written back on
save.

**F5, F6**
: Copy and move between the machine and the other panel, in both directions. A
copy that was interrupted can be resumed.

**F7, F8**
: Create a directory, delete what is marked.

# Shell link settings <!-- help:notitle --><a id="shell-link"></a>

**Shell link settings**

The settings of the plugin, which the Manage plugins dialog of the Options
menu opens.

*Helper*
: Which helper to run on the other machine: the built-in one, which is sent
over the connection, or a script of the directory of helpers. The built-in one
needs nothing installed on the other side.

*Timeouts*
: How long to wait for the connection and for an answer.

*Cache*
: How long a listing that was read is kept before the directory is read again.

The connections and these settings are kept in
*~/.config/mc6/shell-link.ini*.
The helpers that come with the plugin are in
*{{panel_plugins_dir}}/shell-link/helpers*.
