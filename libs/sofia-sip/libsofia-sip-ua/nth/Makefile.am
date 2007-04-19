#
# Makefile.am for nth module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 	-I$(srcdir)/../ipt -I../ipt \
		-I$(srcdir)/../iptsec -I../iptsec \
		-I$(srcdir)/../msg -I../msg \
		-I$(srcdir)/../bnf -I../bnf \
		-I$(srcdir)/../http -I../http \
		-I$(srcdir)/../sresolv -I../sresolv \
		-I$(srcdir)/../tport -I../tport \
		-I$(srcdir)/../url -I../url \
		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libnth.la

check_PROGRAMS = 	test_nth http-client http-server

TESTS = 		test_nth

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		nth_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/nth.h sofia-sip/nth_tag.h

libnth_la_SOURCES = 	nth_client.c nth_server.c nth_tag.c nth_tag_ref.c

COVERAGE_INPUT = 	$(libnth_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libnth.la \
			../iptsec/libiptsec.la \
			../ipt/libipt.la \
			../http/libhttp.la \
			../sresolv/libsresolv.la \
			../tport/libtport.la \
			../stun/libstun.la \
			../sresolv/libsresolv.la \
			../url/liburl.la \
			../msg/libmsg.la \
			../bnf/libbnf.la \
			../su/libsu.la

test_nth_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		nth.docs $(BUILT_SOURCES) \
			agent.pem cafile.pem

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am
