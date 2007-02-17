#
# Makefile.am for url module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 	-I$(srcdir)/../bnf -I../bnf \
		-I$(srcdir)/../ipt -I../ipt \
		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	liburl.la

check_PROGRAMS = 	torture_url test_urlmap

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		url_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/url.h sofia-sip/url_tag.h \
			sofia-sip/url_tag_class.h

liburl_la_SOURCES = 	url.c url_tag.c url_tag_ref.c

COVERAGE_INPUT = 	$(liburl_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		liburl.la \
			../bnf/libbnf.la \
			../su/libsu.la

test_urlmap_SOURCES = 	urlmap.c urlmap.h
test_urlmap_CFLAGS =	$(CFLAGS) -DTEST_URLMAP=1

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		Doxyfile url.docs $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Tests

TESTS = 		torture_url test_urlmap

# ----------------------------------------------------------------------
# Sofia specific rules

include ../sofia.am

TAG_DLL_FLAGS = 	DLLREF=1
