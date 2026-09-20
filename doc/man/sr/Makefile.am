LANG=sr
mandir = @mandir@/$(LANG)

EXTRA_DIST = mc6.1.in mdiff.1.in mcedit6.1.in

man_MANS = mc6.1

if USE_INTERNAL_EDIT
man_MANS += mcedit6.1
endif

if USE_DIFF
man_MANS += mdiff.1
endif

CLEANFILES = $(man_MANS)

DATE_LANG=sr_SR.UTF-8
DATE_FORMAT=%B %Y.

include ../date-of-man-include.am
