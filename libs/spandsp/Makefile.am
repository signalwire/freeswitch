##
## SpanDSP - a series of DSP components for telephony
##
## Makefile.am - Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License version 2.1,
## as published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public
## License along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
## $Id: Makefile.am,v 1.40 2008/07/15 14:28:20 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

noinst_SCRIPTS = spandsp.spec

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = autogen.sh \
             DueDiligence \
             README.testdata \
             spandsp.spec \
             wrapper.xsl \
             libspandsp.vcproj \
             unpack_g722_data.sh \
             unpack_g726_data.sh \
             unpack_gsm0610_data.sh \
             unpack_v56ter_data.sh \
             doc/doxygen.in \
             src/floating_fudge.h \
             src/spandsp/version.h.in \
             src/libspandsp.dsp \
             src/libspandsp.sln \
             src/msvc/gettimeofday.c \
             src/msvc/inttypes.h \
             src/msvc/tgmath.h \
             src/msvc/unistd.h \
             src/msvc/sys/time.h \
             src/msvc/spandsp.def \
             src/msvc/msvcproj.head \
             src/msvc/msvcproj.foot \
             src/msvc/vc8proj.head \
             src/msvc/vc8proj.foot \
             spandsp/global-tones.xml \
             spandsp/tones.dtd \
             spandsp/tsb85.xml \
             debian/changelog \
             debian/compat \
             debian/control \
             debian/copyright \
             debian/libspandsp5.install \
             debian/libspandsp-dev.install \
             debian/libspandsp-doc.install \
             debian/rules \
             debian/watch

if COND_DOC
    MAYBE_DOC=doc
endif
if COND_TESTS
    MAYBE_TESTS=spandsp-sim tests
endif
if COND_TESTDATA
    MAYBE_TESTDATA=test-data
endif
SUBDIRS = src $(MAYBE_DOC) $(MAYBE_TESTS) $(MAYBE_TESTDATA)

DIST_SUBDIRS = src doc spandsp-sim tests test-data

faq: faq.xml
	cd faq ; xsltproc ../wrapper.xsl ../faq.xml

rpm: rpm-build

rpm-build:
	$(MAKE) -$(MAKEFLAGS) bump.rpm.release
	$(MAKE) -$(MAKEFLAGS) dist
	rm -rf rpm/BUILD/*
	rm -f rpm/RPMS/*/*
	rm -f rpm/SOURCES/*
	rm -f rpm/SPECS/*
	rm -f rpm/SRPMS/*
	rpm -ta --sign @PACKAGE@-@VERSION@.tar.gz

bump.rpm.release: spandsp.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'/' \
		<spandsp.spec >spandsp.spec.new; \
	mv spandsp.spec.new spandsp.spec
