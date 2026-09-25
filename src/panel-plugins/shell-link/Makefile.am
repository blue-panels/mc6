SUBDIRS = helpers
DIST_SUBDIRS = helpers

include $(srcdir)/helpers/helpers.mk

shelllinkdir = $(panel_plugins_dir)/shell-link
shelllink_LTLIBRARIES = mc-panel-shell-link.la
shelllink_DATA = shell-link_panel.md

# The protocol library. Not installed and not part of the core: it is compiled
# here and linked into the plugin, so a build without the plugin carries
# neither the code nor the libssh2 dependency.
noinst_LTLIBRARIES = libshfs.la

SHFS_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBSSH2_CFLAGS) \
	-I$(top_srcdir) \
	-I$(builddir) \
	-DSHELL_LINK_HELPERS_DIR=\""$(panel_plugins_dir)/shell-link/helpers"\"

libshfs_la_SOURCES = \
	shfs.c shfs.h shfs-priv.h \
	shfs-log.c \
	shfs-resume.c \
	shell_ssh2.c shell_ssh2.h
nodist_libshfs_la_SOURCES = shelldef.h
libshfs_la_CPPFLAGS = $(SHFS_CPPFLAGS)

mc_panel_shell_link_la_SOURCES = shell-link.c
mc_panel_shell_link_la_CPPFLAGS = $(SHFS_CPPFLAGS) \
	-DMC_PLUGIN_DIR=\""$(panel_plugins_dir)/shell-link"\"
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

# shelldef.h holds every helper script as a C string, so a build that is run
# from the source tree still has them when nothing is installed yet. The text
# is taken from helpers/ at build time: a second, hand-written copy drifts
# away from the scripts that are actually shipped.
BUILT_SOURCES = shelldef.h
CLEANFILES = shelldef.h shelldef.h-t

shelldef.h: $(SHELL_LINK_HELPERS:%=$(srcdir)/helpers/%) $(srcdir)/helpers/helpers.mk
	$(AM_V_GEN){ \
	  echo '/* Generated from helpers/. Do not edit. */'; \
	  echo; \
	  echo '#ifndef MC__SHELLDEF_H'; \
	  echo '#define MC__SHELLDEF_H'; \
	  echo; \
	  for h in $(SHELL_LINK_HELPERS); do \
	    n=`printf '%s' "$$h" | $(SED) \
	        -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	        -e 's/^FEXISTS$$/EXISTS/'`; \
	    printf '#define VFS_SHELL_%s_DEF_CONTENT ( \\\n' "$$n" || exit 1; \
	    $(SED) -e 's/[\\"]/\\&/g' -e 's/.*/    "&\\n" \\/' \
	        "$(srcdir)/helpers/$$h" || exit 1; \
	    printf ')\n\n'; \
	  done; \
	  echo '#endif'; \
	} > $@-t && mv -f $@-t $@
