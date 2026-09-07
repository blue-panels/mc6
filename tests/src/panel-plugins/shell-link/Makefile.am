PACKAGE_STRING = "/src/panel-plugins/shell-link"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/panel-plugins/shell-link/libshfs.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

if ENABLE_SHELL_SSH2
LIBS += $(LIBSSH2_LIBS)
endif

TESTS = \
	shfs_parse_ls \
	helpers_ls.sh

check_PROGRAMS = shfs_parse_ls builtin_ls

shfs_parse_ls_SOURCES = \
	shfs_parse_ls.c

# prints the ls script built into the plugin, for helpers_ls.sh
builtin_ls_SOURCES = \
	builtin_ls.c

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)
AM_TESTS_ENVIRONMENT = srcdir='$(srcdir)'; export srcdir;

EXTRA_DIST = helpers_ls.sh
