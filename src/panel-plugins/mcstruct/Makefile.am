noinst_LTLIBRARIES = libmcslv.la

libmcslv_la_SOURCES = \
	slv.h \
	slv_internal.h \
	slv_expr.c \
	slv_parser.c \
	slv_eval.c \
	slv_format.c \
	slv_reader.c \
	slv_reader.h \
	slv_load.c \
	slv_load.h \
	slv_edit.c \
	slv_edit.h
libmcslv_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\"$(panel_plugins_dir)/mcstruct\"
libmcslv_la_LIBADD = $(GLIB_LIBS) -lm

mcstructdir = $(panel_plugins_dir)/mcstruct
mcstruct_LTLIBRARIES = mc-panel-mcstruct.la
dist_mcstruct_DATA = mcstruct_panel.hlp

mc_panel_mcstruct_la_SOURCES = \
	mcstruct.c \
	mcstruct_ui.c \
	mcstruct_ui_priv.h \
	mcstruct_grid.c \
	mcstruct_cmd.c \
	mcstruct_bytes.c \
	mcstruct_layout.c \
	mcstruct_ui.h
mc_panel_mcstruct_la_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\"$(panel_plugins_dir)/mcstruct\"
mc_panel_mcstruct_la_LDFLAGS = -module -avoid-version
mc_panel_mcstruct_la_LIBADD = libmcslv.la $(GLIB_LIBS)

mcstructdatadir = $(panel_plugins_dir)/mcstruct/data
dist_mcstructdata_DATA = \
	data/mcstruct.ini \
	data/stl.als \
	data/zip.stl \
	data/exe.stl \
	data/dbf.stl \
	data/elf.stl \
	data/mbr.stl \
	data/fat_boot.stl \
	data/uimage.stl \
	data/dtb.stl \
	data/png.stl \
	data/bmp.stl \
	data/wav.stl \
	data/sqlite.stl
