PACKAGE_STRING = "/src/panel-plugins/arcmc"

AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBARCHIVE_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(LIBARCHIVE_LIBS) \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

TESTS = \
	arcmc_ext_registry \
	arcmc_utils

# The reader speaks to libarchive, which the plugin itself may have been
# built without.
if ENABLE_PANEL_PLUGIN_ARCMC
TESTS += arcmc_reader
endif

check_PROGRAMS = $(TESTS)

arcmc_utils_SOURCES = \
	arcmc_utils.c

arcmc_ext_registry_SOURCES = \
	arcmc_ext_registry.c \
	$(top_srcdir)/src/panel-plugins/arcmc/arcmc-ext.c

arcmc_reader_SOURCES = \
	arcmc_reader.c \
	$(top_srcdir)/src/panel-plugins/arcmc/arcmc-reader.c
