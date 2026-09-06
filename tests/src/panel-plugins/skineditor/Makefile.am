PACKAGE_STRING = "/src/panel-plugins/skineditor"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LDADD = @CHECK_LIBS@ \
	$(top_builddir)/src/panel-plugins/skineditor/libmcskinedit.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LDADD += $(GLIB_LIBS) \
	@E2P_LIBS@
endif

TESTS = \
	model_value \
	model_resolve \
	model_save \
	model_chars \
	check_table.sh

check_PROGRAMS = \
	model_value \
	model_resolve \
	model_save \
	model_chars \
	skinedit_table_dump

model_value_SOURCES = model_value.c
model_resolve_SOURCES = model_resolve.c
model_save_SOURCES = model_save.c
model_chars_SOURCES = model_chars.c
skinedit_table_dump_SOURCES = skinedit_table_dump.c

EXTRA_DIST = check_table.sh
