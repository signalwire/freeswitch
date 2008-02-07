##
## VoIPcodecs - a series of DSP components for telephony
##
## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License version 2, as
## published by the Free Software Foundation.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
## $Id: Makefile.am,v 1.29 2007/09/23 12:45:15 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)

noinst_SCRIPTS = voipcodecs.spec

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = voipcodecs.spec \
             wrapper.xsl \
             libvoipcodecs.vcproj \
             doc/libvoipcodecs-doxygen \
             src/float_fudge.h \
             src/voipcodecs/version.h.in \
             src/libvoipcodecs.dsp \
             src/libvoipcodecs.sln \
             src/msvc/gettimeofday.c \
             src/msvc/inttypes.h \
             src/msvc/tgmath.h \
             src/msvc/unistd.h \
             src/msvc/sys/time.h \
             src/msvc/voipcodecs.def \
             src/msvc/msvcproj.head \
             src/msvc/msvcproj.foot \
             src/msvc/vc8proj.head \
             src/msvc/vc8proj.foot \
             debian/changelog \
             debian/compat \
             debian/control \
             debian/copyright \
             debian/libvoipcodecs.install \
             debian/libvoipcodecs-dev.install \
             debian/libvoipcodecs-doc.install \
             debian/rules \
             debian/watch

if COND_DOC
    MAYBE_DOC=doc
endif
if COND_TESTS
    MAYBE_TESTS=tests
endif
if COND_ITUTESTS
    MAYBE_ITUTESTS=itutests
endif
SUBDIRS = src $(MAYBE_DOC) $(MAYBE_TESTS) $(MAYBE_ITUTESTS)

DIST_SUBDIRS = src doc tests localtests etsitests itutests

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

bump.rpm.release: voipcodecs.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'/' \
		<voipcodecs.spec >voipcodecs.spec.new; \
	mv voipcodecs.spec.new voipcodecs.spec
