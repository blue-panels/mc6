mcpeekdir = $(panel_plugins_dir)/mcpeek
mcpeek_LTLIBRARIES = mc-panel-mcpeek.la
dist_mcpeek_DATA = mcpeek_panel.hlp

mc_panel_mcpeek_la_SOURCES = \
	mcpeek.c \
	mcpeek-pe.c mcpeek-pe.h \
	mcpeek-meta.c mcpeek-meta.h \
	mcpeek-sig.c mcpeek-sig.h \
	mcpeek-il.c mcpeek-il.h \
	mcpeek-cs.c mcpeek-cs.h \
	mcpeek-resolve.c mcpeek-resolve.h \
	mcpeek-find.c mcpeek-find.h

mc_panel_mcpeek_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\"$(panel_plugins_dir)/mcpeek\"
mc_panel_mcpeek_la_LDFLAGS = -module -avoid-version
mc_panel_mcpeek_la_LIBADD = $(GLIB_LIBS)
