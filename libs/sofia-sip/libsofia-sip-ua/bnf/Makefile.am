#
# Makefile.am for bnf module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libbnf.la

check_PROGRAMS = 	torture_bnf

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = sofia-sip/bnf.h sofia-sip/hostdomain.h

libbnf_la_SOURCES = 	bnf.c

COVERAGE_INPUT = 	$(libbnf_la_SOURCES) $(include_sofia_HEADERS)

LDADD =			libbnf.la ../su/libsu.la

torture_bnf_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		bnf.docs

# ----------------------------------------------------------------------
# Tests

TESTS = torture_bnf

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

