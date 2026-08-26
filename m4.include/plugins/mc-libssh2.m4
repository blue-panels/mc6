dnl
dnl libssh2, looked for once and shared by everything that needs it.
dnl
dnl Before this existed the check lived in two VFS macros that synchronised
dnl through a variable, so the answer depended on which ran first, and the
dnl panel plugins took their build flags from VFS macros they have nothing to
dnl do with. Results: LIBSSH2_CFLAGS, LIBSSH2_LIBS, have_libssh2.
dnl
dnl 1.9.0 is where ECDSA and ed25519 host keys came in. Older libssh2 only
dnl speaks ssh-rsa and ssh-dss, which OpenSSH 8.8 and later do not offer, so
dnl it connects to no current server.
dnl
AC_DEFUN([mc_LIBSSH2],
[
    have_libssh2=no

    PKG_CHECK_MODULES([LIBSSH2], [libssh2 >= 1.9.0], [have_libssh2=yes], [:])

    AM_CONDITIONAL([HAVE_LIBSSH2], [test x"$have_libssh2" = x"yes"])
])
