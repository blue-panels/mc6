PACKAGE_STRING = "/src/panel-plugins/pp_common"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	pp_rename_with_ext \
	pp_add_entry_st

check_PROGRAMS = $(TESTS)

pp_rename_with_ext_SOURCES = \
	pp_rename_with_ext.c

pp_add_entry_st_SOURCES = \
	pp_add_entry_st.c
