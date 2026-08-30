PACKAGE_STRING = "/src/panel-plugins/mcstruct"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	-DMCSTRUCT_DATA_DIR=\"$(abs_top_srcdir)/src/panel-plugins/mcstruct/data\" \
	@CHECK_CFLAGS@

LDADD = @CHECK_LIBS@ \
	$(top_builddir)/src/panel-plugins/mcstruct/libmcslv.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LDADD += $(GLIB_LIBS) \
	@E2P_LIBS@
endif

TESTS = \
	slv_parse \
	slv_eval \
	slv_reader \
	slv_edit \
	slv_ext \
	slv_defs

check_PROGRAMS = $(TESTS)

noinst_PROGRAMS = mc_slv_dump

slv_parse_SOURCES = slv_parse.c
slv_eval_SOURCES = slv_eval.c
slv_reader_SOURCES = slv_reader.c
slv_edit_SOURCES = slv_edit.c
slv_ext_SOURCES = slv_ext.c
slv_defs_SOURCES = slv_defs.c
mc_slv_dump_SOURCES = mc_slv_dump.c
