DIST_SUBDIRS = es hu it pl ru sr
if USE_NLS
SUBDIRS = $(DOC_LINGUAS)
endif

hlpdir = $(pkgdatadir)/help
hlp_DATA = mcommander.hlp

EXTRA_DIST = xnc.hlp
CLEANFILES = $(hlp_DATA)

mcommander.hlp: $(top_builddir)/doc/man/mcommander.1 $(srcdir)/xnc.hlp $(top_builddir)/src/man2hlp/man2hlp
	- $(top_builddir)/src/man2hlp/man2hlp $(top_builddir)/doc/man/mcommander.1 $(srcdir)/xnc.hlp mcommander.hlp
