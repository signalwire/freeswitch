#
# Makefile.am for nea module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 	-I$(srcdir)/../ipt -I../ipt \
		-I$(srcdir)/../msg -I../msg \
		-I$(srcdir)/../nta -I../nta \
		-I$(srcdir)/../sip -I../sip \
		-I$(srcdir)/../sresolv -I../sresolv \
		-I$(srcdir)/../tport -I../tport \
		-I$(srcdir)/../url -I../url \
		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libnea.la

TESTS =

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		nea_tag_ref.c

nobase_include_sofia_HEADERS =\
			sofia-sip/nea.h sofia-sip/nea_tag.h

libnea_la_SOURCES = 	nea.c nea_event.c \
			nea_server.c nea_debug.h nea_debug.c \
			nea_tag.c nea_tag_ref.c

COVERAGE_INPUT = 	$(libnea_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libnea.la \
			../nta/libnta.la \
			../ipt/libipt.la \
			../sip/libsip.la \
			../sresolv/libsresolv.la \
			../tport/libtport.la \
			../stun/libstun.la \
			../http/libhttp.la \
			../msg/libmsg.la \
			../url/liburl.la \
			../bnf/libbnf.la \
			../su/libsu.la

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		nea.docs $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

