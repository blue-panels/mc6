
noinst_DATA = README.xterm

SCRIPTS_IN = \
	mc6.csh.in \
	mc6.fish.in \
	mc6.sh.in \
	mc6-wrapper.csh.in \
	mc6-wrapper.fish.in \
	mc6-wrapper.sh.in
SCRIPTS_OUT = \
	mc6.csh \
	mc6.fish \
	mc6.sh \
	mc6-wrapper.csh \
	mc6-wrapper.fish \
	mc6-wrapper.sh

pkglibexec_SCRIPTS = $(SCRIPTS_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_ADD)

CLEANFILES = $(SCRIPTS_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(SCRIPTS_IN) \
	$(noinst_DATA)

mc6.csh: $(top_builddir)/config.status $(srcdir)/mc6.csh.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc6.csh.in > mc6.csh

mc6.fish: $(top_builddir)/config.status $(srcdir)/mc6.fish.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc6.fish.in > mc6.fish

mc6.sh: $(top_builddir)/config.status $(srcdir)/mc6.sh.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc6.sh.in > mc6.sh

mc6-wrapper.csh: $(top_builddir)/config.status $(srcdir)/mc6-wrapper.csh.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc6-wrapper.csh.in > mc6-wrapper.csh

mc6-wrapper.fish: $(top_builddir)/config.status $(srcdir)/mc6-wrapper.fish.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc6-wrapper.fish.in > mc6-wrapper.fish

mc6-wrapper.sh: $(top_builddir)/config.status $(srcdir)/mc6-wrapper.sh.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc6-wrapper.sh.in > mc6-wrapper.sh

