#
# Makefile.am for sofia-sip/libsofia-sip-ua
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#
# ref: http://www.gnu.org/software/automake/manual/automake.html

AUTOMAKE_OPTIONS = foreign

# select which optional sofia-sip modules have been enabled
# in the build
OPT_LIBADD =
OPT_SUBDIRS_STUN =
OPT_SUBDIRS_HTTP =
OPT_SUBDIRS_NTH =
if HAVE_STUN
OPT_LIBADD += stun/libstun.la
OPT_SUBDIRS_STUN += stun
endif
if HAVE_NTH
OPT_LIBADD += http/libhttp.la nth/libnth.la
OPT_SUBDIRS_HTTP += http
OPT_SUBDIRS_NTH += nth
endif

# note: order does matter in the subdir list
SUBDIRS = su features bnf sresolv ipt sdp url msg sip $(OPT_SUBDIRS_HTTP) \
	  $(OPT_SUBDIRS_STUN) soa tport nta $(OPT_SUBDIRS_NTH) \
	  nea iptsec nua
DIST_SUBDIRS = su features bnf sresolv ipt sdp url msg sip http \
	  stun soa tport nta nth \
	  nea iptsec nua docs

DOXYGEN = doxygen

noinst_LTLIBRARIES = libsofia-sip-ua.la

libsofia_sip_ua_la_SOURCES =
libsofia_sip_ua_la_LIBADD = 	bnf/libbnf.la \
				features/libfeatures.la \
				ipt/libipt.la \
				iptsec/libiptsec.la \
				msg/libmsg.la \
				nea/libnea.la \
				nta/libnta.la \
				nua/libnua.la \
				sdp/libsdp.la \
				sip/libsip.la \
				soa/libsoa.la \
				sresolv/libsresolv.la \
				su/libsu.la \
				tport/libtport.la \
				url/liburl.la \
				$(OPT_LIBADD)

# set the libtool version info version:revision:age for libsofia-sip-ua
# - soname to 'libsofia-sip-ua.so.(CUR-AGE)'
libsofia_sip_ua_la_LDFLAGS = \
	-version-info $(LIBVER_SOFIA_SIP_UA_CUR):$(LIBVER_SOFIA_SIP_UA_REV):$(LIBVER_SOFIA_SIP_UA_AGE)

PHONY = doxygen built-sources

include $(top_srcdir)/rules/recursive.am

checklib: checklib-recursive $(lib_LTLIBRARIES)

doxygen: built-sources
	@echo Generating empty doxytags
	mkdir -p docs/html ; \
	for d in $(DIST_SUBDIRS); do \
	  test -r $$d/Doxyfile || continue ; \
	  if ! test -r docs/$$d.doxytags ; then \
	    echo '<?xml version="1.0"?><tagfile/>' > docs/$$d.doxytags ; \
	  else \
	    sed '2,10s!<name>index</name>!<name>'$$d'_index</name>!' \
	    docs/$$d.doxytags > docs/$$d.doxytags.tmp && \
	    mv -f docs/$$d.doxytags.tmp docs/$$d.doxytags ; \
          fi ; \
	done
	for d in $(DIST_SUBDIRS); do \
	  ( test -r $$d/Doxyfile && \
	  cd $$d > /dev/null && \
	  echo running ${DOXYGEN} first time in $$d && \
          ${DOXYGEN} 2>&1 ) | \
	  egrep -v -i -e 'Warning: (unsupported (xml/)?html tag|unable to resolve reference|explicit link.*could not be resolved)' ; \
          test -r docs/$$d.doxytags && \
	  sed '2,10s!<name>index</name>!<name>'$$d'_index</name>!' \
	    docs/$$d.doxytags > docs/$$d.doxytags.tmp && \
	  mv -f docs/$$d.doxytags.tmp docs/$$d.doxytags ; \
	done
	for d in $(DIST_SUBDIRS); do \
	  ( test -r $$d/Doxyfile && \
	    cd $$d > /dev/null \
	    echo running ${DOXYGEN} second time in $$d && \
            ${DOXYGEN} 2>&1 ) | \
	    egrep -v -i -e 'Warning: Unsupported (xml/)?html tag' ; \
	  test -r docs/$$d.doxytags && \
	  sed '2,10s!<name>index</name>!<name>'$$d'_index</name>!' \
	    docs/$$d.doxytags > docs/$$d.doxytags.tmp && \
	  mv -f docs/$$d.doxytags.tmp docs/$$d.doxytags ; \
	done
	${top_srcdir}/scripts/hide_emails.sh docs/html

if HAVE_LCOV
include $(top_srcdir)/rules/lcov.am
endif

include $(top_srcdir)/rules/silent.am

.PHONY: $(PHONY)
