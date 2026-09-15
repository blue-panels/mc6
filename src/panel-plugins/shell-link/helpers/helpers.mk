# The helper scripts, named once. The list is read both by helpers/Makefile.am,
# which installs them, and by the rule that compiles them into shelldef.h.

SHELL_LINK_HELPERS = \
	ls mkdir fexists unlink chown chmod rmdir ln mv hardlink \
	get send append info utime putat cksumrange blockdigests
