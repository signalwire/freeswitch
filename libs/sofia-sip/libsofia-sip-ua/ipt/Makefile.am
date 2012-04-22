#
# Makefile.am for ipt module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libipt.la

check_PROGRAMS = 	torture_base64

TESTS = 		torture_base64

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = \
			sofia-sip/base64.h \
			sofia-sip/token64.h \
			sofia-sip/uniqueid.h

libipt_la_SOURCES = 	base64.c token64.c

EXTRA_DIST = 		sofia-sip/utf8.h \
			sofia-sip/rc4.h \
			utf8internal.h \
			utf8.c ucs2.c ucs4.c rc4.c \
			utf8test.c

COVERAGE_INPUT = 	$(libipt_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libipt.la \
			../sresolv/libsresolv.la \
			../su/libsu.la

torture_base64_LDFLAGS = -static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST +=		ipt.docs

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

