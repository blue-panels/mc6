noinst_LTLIBRARIES = libmc-vfs.la

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
libmc_vfs_la_SOURCES = plugins_init.c plugins_init.h

SUBDIRS = local
libmc_vfs_la_LIBADD = local/libvfs-local.la

if ENABLE_VFS_EXTFS
SUBDIRS += extfs
libmc_vfs_la_LIBADD += extfs/libvfs-extfs.la
endif


if ENABLE_VFS_SFS
SUBDIRS += sfs
libmc_vfs_la_LIBADD += sfs/libvfs-sfs.la
endif
