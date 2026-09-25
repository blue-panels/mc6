ftpdir = $(panel_plugins_dir)/ftp
ftp_LTLIBRARIES = mc-panel-ftp.la
ftp_DATA = ftp_panel.md

mc_panel_ftp_la_SOURCES = ftp.c
mc_panel_ftp_la_CPPFLAGS = $(GLIB_CFLAGS) $(LIBCURL_CFLAGS) -I$(top_srcdir) \
	-DMC_PLUGIN_DIR=\""$(panel_plugins_dir)/ftp"\"
mc_panel_ftp_la_LDFLAGS = -module -avoid-version
mc_panel_ftp_la_LIBADD = $(GLIB_LIBS) $(LIBCURL_LIBS)
