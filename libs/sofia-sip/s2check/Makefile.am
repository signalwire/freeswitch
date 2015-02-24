#
# Makefile.am for "Sofia-SIP 2 Check", module and unit testing library
#
# Copyright (C) 2009 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 		$(INTERNAL_INCLUDES)

# ----------------------------------------------------------------------
# Build targets

noinst_LIBRARIES = 	libs2.a

# ----------------------------------------------------------------------
# Rules for building the targets

libs2_a_SOURCES = 	s2check.h s2tcase.c \
			s2base.h s2base.c \
			s2sip.c s2sip.h \
			s2_localinfo.h s2_localinfo.c \
			s2dns.h s2dns.c \
			s2util.h s2time.c

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =

# ----------------------------------------------------------------------
# Tests

TESTS =

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am
