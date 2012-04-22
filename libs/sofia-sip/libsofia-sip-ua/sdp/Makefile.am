#
# Makefile.am for sdp module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsdp.la

check_PROGRAMS = 	torture_sdp test_sdp

dist_noinst_SCRIPTS =	run_test_sdp

TESTS = 		torture_sdp run_test_sdp

BUILT_SOURCES =		sdp_tag_ref.c

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = \
			sofia-sip/sdp.h sofia-sip/sdp_tag.h

libsdp_la_SOURCES = 	sdp.c sdp_parse.c sdp_print.c \
			sdp_tag.c sdp_tag_ref.c

COVERAGE_INPUT = 	$(libsdp_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libsdp.la \
			../su/libsu.la

test_sdp_LDFLAGS = 	-static
torture_sdp_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		sdp.docs $(BUILT_SOURCES) \
			tests/message-1.sdp \
			tests/message-2.sdp \
			tests/message-3.sdp \
			tests/message-4.sdp \
			tests/message-5.sdp \
			tests/message-6.sdp \
			tests/message-7.sdp \
			tests/message-8.sdp \
			tests/message-9.sdp \
			tests/message-10.sdp \
			tests/message-11.sdp

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

