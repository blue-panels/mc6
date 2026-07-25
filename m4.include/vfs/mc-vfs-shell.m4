dnl Enable SHELL protocol
AC_DEFUN([mc_VFS_SHELL],
[
    AC_ARG_ENABLE([vfs-shell],
		    AS_HELP_STRING([--enable-vfs-shell], [Support for SHELL filesystem @<:@yes@:>@]))

    have_shell_ssh2=no
    AC_ARG_ENABLE([shell-ssh2],
		    AS_HELP_STRING([--enable-shell-ssh2@<:@=auto/yes/no@:>@],
				   [Use libssh2 as Shell VFS transport (default: auto)]),
		    [enable_shell_ssh2="$enableval"],
		    [enable_shell_ssh2=auto])

    AS_CASE([$enable_shell_ssh2],
	[yes|no|auto], [],
	[AC_MSG_ERROR([bad value '$enable_shell_ssh2' for --enable-shell-ssh2 (use auto, yes, or no)])])

    if test "$enable_vfs" = "yes" -a "x$enable_vfs_shell" != xno; then
	enable_vfs_shell="yes"
	mc_VFS_ADDNAME([shell])
	AC_DEFINE([ENABLE_VFS_SHELL], [1], [Support for SHELL vfs])

	dnl Optional libssh2 transport for Shell VFS (password auth, native SSH)
	if test x"$enable_shell_ssh2" != xno; then
	    if test x"$found_libssh" != "xyes"; then
		PKG_CHECK_MODULES(LIBSSH, [libssh2 >= 1.2.8], [found_libssh=yes], [:])
	    fi
	    if test x"$found_libssh" = "xyes"; then
		AC_DEFINE([ENABLE_SHELL_SSH2], [1], [Use libssh2 for Shell VFS transport])
		have_shell_ssh2=yes
	    elif test x"$enable_shell_ssh2" = xyes; then
		AC_MSG_ERROR([Shell VFS libssh2 transport requested but libssh2 >= 1.2.8 was not found.
Install libssh2 development package (for example: libssh2-1-dev) or use --enable-shell-ssh2=auto/no.])
	    fi
	fi
    fi
    AM_CONDITIONAL(ENABLE_VFS_SHELL, [test "$enable_vfs" = "yes" -a x"$enable_vfs_shell" = x"yes"])
    AM_CONDITIONAL(ENABLE_SHELL_SSH2, [test x"$have_shell_ssh2" = x"yes"])
])
