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
