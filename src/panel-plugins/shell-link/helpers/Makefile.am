# The helper scripts are the plugin's, not the file manager's, so they install
# beside it. The per-host override in ~/.local/share/mc6/shell/ is unchanged.
shelllinkhelpersdir = $(panel_plugins_dir)/shell-link/helpers

SHELL_MISC = README.shell

shelllinkhelpers_DATA = $(SHELL_MISC) \
	ls mkdir fexists unlink chown chmod rmdir ln mv hardlink \
	get send append info utime putat cksumrange blockdigests

EXTRA_DIST = $(shelllinkhelpers_DATA)
