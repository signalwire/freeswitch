##
## broadvoice - a library for the BroadVoice 16 and 32 codecs
##
## Makefile.am -- Process this file with automake to produce Makefile.in
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the Lesser GNU General Public License version 2.1, as
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
## $Id: Makefile.am,v 1.2 2009/11/21 17:19:22 steveu Exp $

AM_CFLAGS = $(COMP_VENDOR_CFLAGS)
AM_LDFLAGS = $(COMP_VENDOR_LDFLAGS)

noinst_SCRIPTS = broadvoice.spec

MAINTAINERCLEANFILES = Makefile.in

EXTRA_DIST = autogen.sh \
             broadvoice.pc \
             broadvoice.spec \
             wrapper.xsl \
             doc/broadvoice-doxygen \
             debian/changelog \
             debian/compat \
             debian/control \
             debian/copyright \
             debian/libbroadvoice1.install \
             debian/libbroadvoice-dev.install \
             debian/libbroadvoice-doc.install \
             debian/rules \
             debian/watch

if COND_DOC
    MAYBE_DOC=doc
endif
if COND_TESTS
    MAYBE_TESTS=tests
endif
SUBDIRS = src $(MAYBE_DOC) $(MAYBE_TESTS)

DIST_SUBDIRS = src doc tests test-data

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = broadvoice.pc

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

bump.rpm.release: broadvoice.spec
	VERSION="x"; \
	test -f $(srcdir)/rpm.release && . $(srcdir)/rpm.release; \
	NEXT_RELEASE=0; \
	test "$$VERSION" = "@VERSION@" && NEXT_RELEASE="$$RELEASE"; \
	RELEASE=`expr $$NEXT_RELEASE + 1`; \
	echo "VERSION=@VERSION@" >$(srcdir)/rpm.release; \
	echo "RELEASE=$$RELEASE" >>$(srcdir)/rpm.release; \
	sed 's/^Release: .*/Release: '$$RELEASE'/' \
		<broadvoice.spec >broadvoice.spec.new; \
	mv broadvoice.spec.new broadvoice.spec
