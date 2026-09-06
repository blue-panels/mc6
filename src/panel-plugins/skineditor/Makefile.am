noinst_LTLIBRARIES = libmcskinedit.la

libmcskinedit_la_SOURCES = \
	skinedit_model.c \
	skinedit_model.h \
	skinedit_table.c \
	skinedit_table.h
libmcskinedit_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)
libmcskinedit_la_LIBADD = $(GLIB_LIBS)

skineditordir = $(panel_plugins_dir)/skineditor
skineditor_LTLIBRARIES = mc-panel-skineditor.la

mc_panel_skineditor_la_SOURCES = \
	skineditor.c \
	skineditor_ui.c \
	skineditor_ui.h
mc_panel_skineditor_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\"$(panel_plugins_dir)/skineditor\"
mc_panel_skineditor_la_LDFLAGS = -module -avoid-version
mc_panel_skineditor_la_LIBADD = libmcskinedit.la $(GLIB_LIBS)
