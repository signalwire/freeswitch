#
# Makefile.am for sofia-sip/libsofia-sip-ua
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#
# ref: http://www.gnu.org/software/automake/manual/automake.html

AUTOMAKE_OPTIONS = foreign

# note: order does matter in the subdir list
SUBDIRS = su features bnf sresolv sdp url msg sip stun ipt soa \
	  tport http nta nea iptsec nth nua
DIST_SUBDIRS = $(SUBDIRS) docs

EXTRA_DIST = sofia.am
DOXYGEN = doxygen

lib_LTLIBRARIES = libsofia-sip-ua.la

libsofia_sip_ua_la_SOURCES = 
libsofia_sip_ua_la_LIBADD = 	bnf/libbnf.la \
				features/libfeatures.la \
				ipt/libipt.la \
				iptsec/libiptsec.la \
				msg/libmsg.la \
				nea/libnea.la \
				nta/libnta.la \
				nth/libnth.la \
				nua/libnua.la \
				http/libhttp.la \
				sdp/libsdp.la \
				sip/libsip.la \
				soa/libsoa.la \
				sresolv/libsresolv.la \
				su/libsu.la \
				stun/libstun.la \
				tport/libtport.la \
				url/liburl.la
# set the libtool version info version:revision:age for libsofia-sip-ua
# - soname to 'libsofia-sip-ua.so.(CUR-AGE)'
libsofia_sip_ua_la_LDFLAGS = \
	-version-info $(LIBVER_SOFIA_SIP_UA_CUR):$(LIBVER_SOFIA_SIP_UA_REV):$(LIBVER_SOFIA_SIP_UA_AGE)

if ENABLE_COVERAGE
COVERAGE_RECURSIVE = coverage-recursive
coverage: $(COVERAGE_RECURSIVE)
endif

all-recursive: built-sources-recursive
built-sources: built-sources-recursive 
clean-built-sources: clean-built-sources-recursive

built-sources-recursive clean-built-sources-recursive $(COVERAGE_RECURSIVE):
	target=`echo $@ | sed s/-recursive//`; \
	list='$(DIST_SUBDIRS)'; for subdir in $$list; do \
	  echo "Making $@ in $$subdir"; \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$target) \
	done;

doxygen: built-sources
	@echo Generating empty doxytags
	cd ${srcdir}; \
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
	@cd ${srcdir} ;\
	for d in $(DIST_SUBDIRS); do \
	  test -r $$d/Doxyfile \
	  && pushd $$d > /dev/null \
	  && echo running ${DOXYGEN} first time in $$d \
          && ${DOXYGEN} 2>&1 \
	  | egrep -v -i -e 'Warning: (unsupported (xml/)?html tag|unable to resolve reference|could not be resolved)' \
          ; popd > /dev/null ; \
	  test -r docs/$$d.doxytags && \
	  sed '2,10s!<name>index</name>!<name>'$$d'_index</name>!' \
	    docs/$$d.doxytags > docs/$$d.doxytags.tmp && \
	  mv -f docs/$$d.doxytags.tmp docs/$$d.doxytags ; \
	done 
	@cd ${srcdir} ;\
	for d in $(DIST_SUBDIRS); do \
	  test -r $$d/Doxyfile \
	  && pushd $$d > /dev/null \
	  && echo running ${DOXYGEN} in second time in $$d \
          && ${DOXYGEN} 2>&1 \
	   | egrep -v -i -e 'Warning: Unsupported (xml/)?html tag' \
          ; popd > /dev/null ; \
	  test -r docs/$$d.doxytags && \
	  sed '2,10s!<name>index</name>!<name>'$$d'_index</name>!' \
	    docs/$$d.doxytags > docs/$$d.doxytags.tmp && \
	  mv -f docs/$$d.doxytags.tmp docs/$$d.doxytags ; \
	done 
	cd ${srcdir}/docs/html && ../hide_emails.sh

.PHONY: built-sources built-sources-am empty-doxytags doxygen 
