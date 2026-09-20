## Process this file with automake to create Makefile.in.

SUBDIRS = ext.d macros.d skins syntax

LIBFILES_OUT = extensions.ini magic.ini

noinst_DATA = xterm.ad

PKGDATA_IN = charsets.in

PKGDATA_OUT = charsets

dist_pkgdata_DATA = \
	defaults.ini

pkgdata_DATA = \
	$(PKGDATA_OUT)

SCRIPTS_IN = \
	mcedit.menu.in

SCRIPTS_OUT = \
	mcedit.menu

LIBFILES_CONST = \
	filehighlight.ini \
	panels.ftp.ini \
	panels.git.ini \
	keymap.default.ini \
	keymap.emacs.ini \
	keymap.vim.ini \
	spell.keymap \
	ctags.keymap


LIBFILES_SCRIPT = \
	edit.indent.rc

CLEANFILES = $(SCRIPTS_OUT) $(PKGDATA_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_CONST) \
	$(SCRIPTS_OUT) \
	$(LIBFILES_OUT)

cfg_SCRIPTS = \
	$(LIBFILES_SCRIPT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(LIBFILES_CONST) \
	$(LIBFILES_SCRIPT) \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(PKGDATA_IN)

install-data-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f keymap.ini && $(LN_S) keymap.default.ini keymap.ini

uninstall-hook:
	cd $(DESTDIR)$(cfgdir) && rm -f keymap.ini
