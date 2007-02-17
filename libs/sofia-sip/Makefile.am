#
# Makefile.am for sofia-sip package
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

AUTOMAKE_OPTIONS = foreign 1.7

SUBDIRS = libsofia-sip-ua $(GLIB_SUBDIRS) utils packages
DIST_SUBDIRS = libsofia-sip-ua libsofia-sip-ua-glib utils packages win32

# note: when glib devel files are not available, make should not
#       enter the libsofia-sip-ua-glib subdir at all
if HAVE_GLIB
GLIB_SUBDIRS = libsofia-sip-ua-glib
endif

PACKAGE = @PACKAGE@
VERSION = @VERSION@

EXTRA_DIST =	AUTHORS COPYING COPYRIGHTS ChangeLog.ext-trees \
		README README.developers RELEASE TODO 

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST += 	m4/sac-general.m4 m4/sac-su.m4 \
		m4/sac-su2.m4 m4/sac-tport.m4 m4/sac-openssl.m4

dist_man_MANS = man/man1/sip-date.1 man/man1/sip-options.1 \
		man/man1/localinfo.1 man/man1/addrinfo.1 \
		man/man1/stunc.1 man/man1/sip-dig.1

$(dist_man_MANS): manpages

manpages: built-sources
	-mkdir -p man man/man1 2> /dev/null
if HAVE_DOXYGEN
	cd utils && $(DOXYGEN) Doxyfile.build
	@rm -f man/man1/_*.1
else
	-touch $(dist_man_MANS)
endif

CLEANFILES = $(dist_man_MANS)

coverage built-sources clean-built-sources doxygen:
	for i in libsofia-sip-ua $(GLIB_SUBDIRS) ; do $(MAKE) $(AM_MAKEFLAGS) -C $$i $@ ; done

.PHONY: coverage built-sources clean-built-sources doxygen manpages
