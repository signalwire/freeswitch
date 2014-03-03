#
# Makefile.am for soa module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 		-I$(srcdir)/../sdp -I../sdp \
			-I$(srcdir)/../sip -I../sip \
			-I$(srcdir)/../msg -I../msg \
			-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../ipt -I../ipt \
			-I$(srcdir)/../bnf -I../bnf \
			-I$(srcdir)/../su -I../su \
			-I$(top_srcdir)/s2check

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsoa.la

check_PROGRAMS =	test_soa

TESTS = 		test_soa

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		soa_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/soa.h sofia-sip/soa_session.h \
			sofia-sip/soa_add.h sofia-sip/soa_tag.h

libsoa_la_SOURCES = 	soa.c soa_static.c \
			soa_tag.c soa_tag_ref.c

COVERAGE_INPUT = 	$(libsoa_la_SOURCES) $(include_sofia_HEADERS)

test_soa_LDADD = 	$(top_builddir)/s2check/libs2.a \
			libsoa.la \
			../sip/libsip.la \
			../msg/libmsg.la \
			../url/liburl.la \
			../sdp/libsdp.la \
			../ipt/libipt.la \
			../bnf/libbnf.la \
			../su/libsu.la

test_soa_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		soa.docs $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

TAG_DLL_FLAGS =		LIST=soa_tag_list
