#
# Makefile.am @template@ for stun module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# General options

AUTOMAKE_OPTIONS = 	foreign

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		$(INTERNAL_INCLUDES)

# ----------------------------------------------------------------------
# Build targets

stun_lib_sources = 	stun.c stun_common.c stun_dns.c stun_mini.c

noinst_LTLIBRARIES = 	libstun.la

bin_PROGRAMS    =       stunc
check_PROGRAMS = 	lookup_stun_server

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = \
			sofia-sip/stun.h sofia-sip/stun_common.h \
			sofia-sip/stun_tag.h
libstun_la_SOURCES = 	$(stun_lib_sources) \
			stun_tag.c stun_tag_ref.c stun_internal.h

BUILT_SOURCES =		stun_tag_ref.c

COVERAGE_INPUT = 	$(libstun_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libstun.la ../sresolv/libsresolv.la ../su/libsu.la

# ----------------------------------------------------------------------
# tests

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		stun.docs cert.pem key.pem

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

TAG_DLL_FLAGS =		LIST=stun_tag_list
