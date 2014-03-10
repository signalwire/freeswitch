#
# Makefile.am for su-glib module
#
# Copyright (C) 2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.
#

# ----------------------------------------------------------------------
# Headers

S_BASE = $(top_srcdir)/libsofia-sip-ua
B_BASE = $(top_builddir)/libsofia-sip-ua

AM_CPPFLAGS =		-I$(S_BASE)/su -I$(B_BASE)/su $(GLIB_CFLAGS) \
			$(SOFIA_GLIB_CFLAGS)

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsu-glib.la

check_PROGRAMS = 	su_source_test torture_su_glib_timer

# ----------------------------------------------------------------------
# Tests

TESTS = 		su_source_test torture_su_glib_timer

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS =
if HAVE_GLIB
nobase_include_sofia_HEADERS += \
			sofia-sip/su_source.h \
			sofia-sip/su_glib.h
endif

libsu_glib_la_SOURCES = su_source.c

libsu_glib_la_DEPENDENCIES = \
			../../libsofia-sip-ua/libsofia-sip-ua.la

LDADD = 		libsu-glib.la \
			../../libsofia-sip-ua/libsofia-sip-ua.la \
			$(GLIB_LIBS)

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		Doxyfile su_glib.docs

# ----------------------------------------------------------------------
# Automake options

AUTOMAKE_OPTIONS = 	foreign

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am
