DIST_SUBDIRS = es hu it pl ru sr
if USE_NLS
SUBDIRS = $(DOC_LINGUAS)
endif

man_MANS = mc6.1 mview.1

if USE_INTERNAL_EDIT
man_MANS += mcedit6.1
endif

if USE_DIFF
man_MANS += mdiff.1
endif

CLEANFILES = $(man_MANS)

EXTRA_DIST = \
	date-of-man-include.am \
	mc6.1.in \
	mcedit6.1.in \
	mview.1.in \
	mdiff.1.in

DATE_LANG=en_US.UTF-8
DATE_FORMAT=%B %Y

include date-of-man-include.am
