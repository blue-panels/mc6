# alpine-remote

mc runs in the debian-12 image; the remote host is Alpine 3.22: musl, busybox
`ls`, `stat` and `find` on the far side of shell-link, and openssh, vsftpd and
samba as apk ships them.

    tests/misc/docker/sandbox.sh alpine-remote up
    tests/misc/docker/sandbox.sh alpine-remote test -w sftp,sh,ftp,smb

What it is meant to catch: a helper that assumes GNU options, a listing that
parses differently, an sshd with another set of algorithms.
