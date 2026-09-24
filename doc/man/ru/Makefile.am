LANG=ru
mandir = @mandir@/$(LANG)

EXTRA_DIST = roff/mcommander.1.in mcommander.md

man_MANS = mcommander.1

CLEANFILES = $(man_MANS)

include ../man-include.am

mcommander.1: $(srcdir)/roff/mcommander.1.in
	$(SED) $(SED_PARAMETERS) '$(srcdir)/roff/mcommander.1.in' > '$@'
