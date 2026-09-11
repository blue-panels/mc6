PACKAGE_STRING = "/src/syntax"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/syntax/libmcsyntax.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = line_local

check_PROGRAMS = $(TESTS)

line_local_SOURCES = \
	line_local.c
