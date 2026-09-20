SUBDIRS = syntax mctree filemanager man2hlp vfs viewer panel-plugins

if ENABLE_LUA_PLUGIN
SUBDIRS += lua
endif

if ENABLE_MCTERM
SUBDIRS += mcterm
endif

if USE_INTERNAL_EDIT
SUBDIRS += editor
endif

if USE_DIFF
SUBDIRS += diffviewer
endif


noinst_LTLIBRARIES = libinternal.la

AM_CPPFLAGS = \
	-DSYSCONFDIR=\""$(sysconfdir)/@PACKAGE@/"\" \
	-DLIBEXECDIR=\""$(libexecdir)/@PACKAGE@/"\" \
	-DDATADIR=\""$(pkgdatadir)/"\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DEXTHELPERSDIR=\""@EXTHELPERSDIR@/"\" \
	-DMC_PANEL_PLUGINS_DIR=\""$(panel_plugins_dir)"\" \
	-DMC_EDITOR_PLUGINS_DIR=\""$(editor_plugins_dir)"\" \
	-DMC_RUNTIME_PLUGINS_DIR=\""$(runtime_plugins_dir)"\"

if ENABLE_LUA_PLUGIN
AM_CPPFLAGS += \
	-DMC_LUA_SYSTEM_SCRIPTS_DIR=\""$(lua_scripts_dir)"\" \
	-DMC_LUA_SYSTEM_MODULES_DIR=\""$(lua_modules_dir)"\"
endif

if CONS_SAVER
SUBDIRS += consaver
AM_CPPFLAGS += -DSAVERDIR=\""$(pkglibexecdir)"\"
endif

AM_CPPFLAGS += -I$(top_srcdir) $(GLIB_CFLAGS)

bin_PROGRAMS = mc6

if USE_INTERNAL_EDIT
EDITLIB = editor/libedit.la
endif

if USE_DIFF
DIFFLIB = diffviewer/libdiffviewer.la
endif


if ENABLE_MCTERM
MCTERMLIB = mcterm/libmcterm.la
endif

libinternal_la_LIBADD = \
	syntax/libmcsyntax.la \
	mctree/libmctree.la \
	filemanager/libmcfilemanager.la \
	vfs/libmc-vfs.la \
	viewer/libmcviewer.la \
	$(MCTERMLIB) \
	$(DIFFLIB) $(EDITLIB)

if HAVE_GMODULE
mc6_LDFLAGS = -export-dynamic
endif

mc6_LDADD = \
	libinternal.la

if ENABLE_MCLIB
libinternal_la_LIBADD += \
	$(top_builddir)/lib/libmc.la
else
mc6_LDADD += \
	$(top_builddir)/lib/libmc.la
endif

SRC_mc_conssaver = \
	cons.handler.c consaver/cons.saver.h

mc6_SOURCES = \
	main.c

libinternal_la_SOURCES = \
	$(SRC_mc_conssaver) \
	args.c args.h \
	clipboard.c clipboard.h \
	events_init.c events_init.h \
	execute.c execute.h \
	file_history.c file_history.h \
	help.c help.h \
	history.h \
	keybind_dialog.c keybind_dialog.h \
	key_sniffer.c key_sniffer.h \
	keymap.c keymap.h \
	key_learn.c key_learn.h \
	manage_plugins.c manage_plugins.h \
	runtime-host.c runtime-host.h \
	runtime-panel-provider.c runtime-panel-provider.h \
	runtime-screen.c runtime-screen.h \
	runtime-viewer-source.c runtime-viewer-source.h \
	selcodepage.c selcodepage.h \
	setup.c setup.h \
	textconf.c textconf.h \
	usermenu.c usermenu.h \
	usermenu_ini.c usermenu_ini.h \
	util.c util.h

if ENABLE_BACKGROUND
    libinternal_la_SOURCES += background.c background.h
endif

EXTRA_DIST = $(SRC_maintainer) $(SRC_charset)

# end of automated testing

install-exec-hook:
	$(MAKE) install_mview
	$(MAKE) install_mctree
	$(MAKE) install_mcstruct
if USE_INTERNAL_EDIT
	$(MAKE) install_mcedit6
endif
if USE_DIFF
	$(MAKE) install_mdiff
endif

#
# Make relative symlinks, but do the right thing if LN_S is `ln' or `cp'.
#
install_mview:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mview && $(LN_S) mc6 mview

install_mctree:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mctree && $(LN_S) mc6 mctree

install_mcstruct:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcstruct && $(LN_S) mc6 mcstruct

install_mcedit6:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mcedit6 && $(LN_S) mc6 mcedit6

install_mdiff:
	cd $(DESTDIR)$(bindir)/$(binprefix) && rm -f mdiff && $(LN_S) mc6 mdiff

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mview
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mctree
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcstruct
if USE_INTERNAL_EDIT
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mcedit6
endif
if USE_DIFF
	rm -f $(DESTDIR)$(bindir)/$(binprefix)/mdiff
endif
