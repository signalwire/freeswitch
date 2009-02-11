#
# Makefile.am for Sofia-SIP module and unit testing
#
# Copyright (C) 2009 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		$(INTERNAL_INCLUDES)

# ----------------------------------------------------------------------
# Build targets

noinst_LIBRARIES = 	libs2.a

# ----------------------------------------------------------------------
# Rules for building the targets

libs2_a_SOURCES = 	s2check.h s2tcase.c \
			s2_localinfo.h s2_localinfo.c

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =

# ----------------------------------------------------------------------
# Tests

TESTS =

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am
