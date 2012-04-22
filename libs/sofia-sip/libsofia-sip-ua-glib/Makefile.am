#
# Makefile.am for sofia-sip/libsofia-sip-ua-glib
#
# Copyright (C) 2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#

AUTOMAKE_OPTIONS = foreign 1.7

SUBDIRS=su-glib

GLIB_TARGETS=su-glib/libsu-glib.la

lib_LTLIBRARIES =
if HAVE_GLIB
lib_LTLIBRARIES += libsofia-sip-ua-glib.la
endif

libsofia_sip_ua_glib_la_SOURCES =
libsofia_sip_ua_glib_la_LIBADD = $(GLIB_TARGETS) $(GLIB_LIBS)

# set the libtool version info version:revision:age for libsofia-sip-ua-glib
# - soname to 'libsofia-sip-ua-glib.so.(CUR-AGE)'
libsofia_sip_ua_glib_la_LDFLAGS = \
	-version-info $(LIBVER_SOFIA_SIP_UA_GLIB_CUR):$(LIBVER_SOFIA_SIP_UA_GLIB_REV):$(LIBVER_SOFIA_SIP_UA_GLIB_AGE)

DOXYGEN = doxygen

EXTRA_DIST = docs/Doxyfile.aliases \
	     docs/Doxyfile.conf \
	     docs/Doxyfile.version

doxygen: built-sources
	@mkdir -p docs docs/html &&\
	for d in $(DIST_SUBDIRS) $(DIST_SUBDIRS); do \
	  test -r $$d/Doxyfile \
	  && pushd $$d > /dev/null \
	  && echo running ${DOXYGEN} in $$d \
          && ${DOXYGEN} \
          && popd > /dev/null ; \
	done
	${top_srcdir}/scripts/hide_emails.sh docs/html

PHONY = doxygen

include $(top_srcdir)/rules/recursive.am

if HAVE_LCOV
include $(top_srcdir)/rules/lcov.am
endif

include $(top_srcdir)/rules/silent.am

.PHONY = $(PHONY)

CLEANFILES = docs/*.doxytags

distclean-local:
	-rm -rf docs/html
