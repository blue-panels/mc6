# debian-10

The lower bound: Debian 10 has GLib 2.58.3, the oldest version configure.ac
accepts, and the rest of its libraries are as old: libarchive 3.3, curl 7.64
(too old for the s3 plugin, which configure should leave out by itself),
libssh2 1.8, gcc 8, gettext 0.19, check 0.10.

    tests/misc/docker/sandbox.sh debian-10 up
    tests/misc/docker/sandbox.sh debian-10 test -w local,sftp

What it is meant to catch: a GLib call newer than 2.58 (the build fails,
which is the point), a libarchive feature that 3.3 does not have, and
whatever the panel plugins assume about the age of their libraries.

Found here: libssh2 1.8 (the one Debian 10 has) knows only the ssh-rsa and
ssh-dss host key algorithms, which OpenSSH 8.8 and later do not offer, so the
sftp and shell-link plugins cannot connect to a current server from this
environment -- and the sftp plugin closes its progress box without a word
about it. configure asks for libssh2 >= 1.2.8; ECDSA and ed25519 host keys
need 1.9. Recorded in expect.tsv as known, over sftp and sh.
