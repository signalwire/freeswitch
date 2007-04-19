#
# Makefile.am for sofia features module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES =		$(INTERNAL_INCLUDES)

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libfeatures.la

# ----------------------------------------------------------------------
# Rules for building the targets

# This is used by platforms not supporting autoconf
nobase_include_sofia_HEADERS = sofia-sip/sofia_features.h

libfeatures_la_SOURCES = features.c

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		features.docs

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

