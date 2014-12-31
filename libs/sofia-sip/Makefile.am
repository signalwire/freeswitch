#
# Makefile.am for sofia-sip package
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

AUTOMAKE_OPTIONS = foreign 1.7 subdir-objects

SUBDIRS =  libsofia-sip-ua $(GLIB_SUBDIRS) packages # tests s2check utils
DIST_SUBDIRS = s2check libsofia-sip-ua libsofia-sip-ua-glib utils packages \
	tests win32 open_c

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

EXTRA_DIST += 	m4/sac-general.m4 m4/sac-coverage.m4 \
		m4/sac-su2.m4 m4/sac-tport.m4 m4/sac-openssl.m4

EXTRA_DIST +=   docs/build_system.txt \
		docs/devel_platform_notes.txt \
		docs/release_management.txt

EXTRA_DIST +=   scripts/lcov-report scripts/uncovered \
		scripts/hide_emails.sh

dist_man_MANS = 
#		man/man1/sip-date.1 man/man1/sip-options.1 \
#		man/man1/localinfo.1 man/man1/addrinfo.1 \
#		man/man1/stunc.1 man/man1/sip-dig.1

noop:
	@echo ok

$(dist_man_MANS): manpages

manpages:
	-mkdir -p man man/man1 2> /dev/null
if HAVE_DOXYGEN 
	$(MAKE) $(AM_MAKEFLAGS) -C libsofia-sip-ua/docs built-sources
	@echo 'cd utils && $(DOXYGEN)'
	@cd utils && \
	{ exec 3>&1 1>&2; { $(DOXYGEN) 2>&1; echo $$? >& 3 ;} | \
	  fgrep -v 'Warning: explicit' ;} | { read x; exit $$x ;}
	@rm -f man/man1/_*.1
else
	-touch $(dist_man_MANS)
endif

CLEANFILES = $(dist_man_MANS)

built-sources clean-built-sources valcheck doxygen:
	@failcom='exit 1'; for f in x $$MAKEFLAGS; do \
	case $$f in *=* | --[!k]*);; *k*) failcom='fail=yes';; esac; done; \
	for i in libsofia-sip-ua $(GLIB_SUBDIRS) ; do \
	  (cd $$i && $(MAKE) $(AM_MAKEFLAGS) $@) || eval $$failcom; \
	done ; \
	test -z "$$fail"

PHONY = built-sources clean-built-sources valcheck doxygen manpages

# s2check depends on generated headers within libsofia-sip-ua
all check: built-sources

if HAVE_LCOV

include $(top_srcdir)/rules/lcov.am

lcov-upload: lcov
	rsync -rvz -e ssh --delete lcov/* sofia-sip.org:/var/www/coverage/lcov/

endif

.PHONY: $(PHONY)
