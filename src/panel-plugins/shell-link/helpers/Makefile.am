# The helper scripts are the plugin's, not the file manager's, so they install
# beside it. The per-host override in ~/.local/share/mc6/shell/ is unchanged.
shelllinkhelpersdir = $(panel_plugins_dir)/shell-link/helpers

include $(srcdir)/helpers.mk

SHELL_MISC = README.shell

shelllinkhelpers_DATA = $(SHELL_MISC) $(SHELL_LINK_HELPERS)

EXTRA_DIST = $(shelllinkhelpers_DATA)
