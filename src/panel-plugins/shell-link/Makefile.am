SUBDIRS = helpers
DIST_SUBDIRS = helpers

shelllinkdir = $(panel_plugins_dir)/shell-link
shelllink_LTLIBRARIES = mc-panel-shell-link.la

# The protocol library. Not installed and not part of the core: it is compiled
# here and linked into the plugin, so a build without the plugin carries
# neither the code nor the libssh2 dependency.
noinst_LTLIBRARIES = libshfs.la

SHFS_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBSSH2_CFLAGS) \
	-I$(top_srcdir) \
	-DSHELL_LINK_HELPERS_DIR=\""$(panel_plugins_dir)/shell-link/helpers"\"

libshfs_la_SOURCES = \
	shfs.c shfs.h shfs-priv.h \
	shfs-log.c \
	shfs-resume.c \
	shell_ssh2.c shell_ssh2.h \
	shelldef.h
libshfs_la_CPPFLAGS = $(SHFS_CPPFLAGS)

mc_panel_shell_link_la_SOURCES = shell-link.c
mc_panel_shell_link_la_CPPFLAGS = $(SHFS_CPPFLAGS)
mc_panel_shell_link_la_LDFLAGS = -module -avoid-version
mc_panel_shell_link_la_LIBADD = libshfs.la $(GLIB_LIBS)

if ENABLE_SHELL_SSH2
mc_panel_shell_link_la_LIBADD += $(LIBSSH2_LIBS)
endif

# Proof that the library stands on its own: it links libshfs and mc's support
# code, and nothing of the file manager. Not installed - it exists to be run by
# hand when it is unclear whether a fault is in the protocol or in the plugin
# that drives it.
noinst_PROGRAMS = shfs-tool

shfs_tool_SOURCES = shfs-tool.c
shfs_tool_CPPFLAGS = $(SHFS_CPPFLAGS)
shfs_tool_LDADD = \
	libshfs.la \
	$(top_builddir)/lib/libmc.la \
	$(GLIB_LIBS)

if ENABLE_SHELL_SSH2
shfs_tool_LDADD += $(LIBSSH2_LIBS)
endif
