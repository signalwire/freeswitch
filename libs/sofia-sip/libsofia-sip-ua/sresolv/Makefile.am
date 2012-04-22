#
# Makefile.am for sresolv module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../bnf -I../bnf \
			-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsresolv.la

check_PROGRAMS = 	torture_sresolv test_sresolv

TESTS =

check_PROGRAMS +=	check_sres_sip
TESTS +=		check_sres_sip
check_sres_sip_LDADD = 	$(LDADD) ${top_builddir}/s2check/libs2.a @CHECK_LIBS@
INCLUDES +=		-I${top_srcdir}/s2check

TESTS += 		torture_sresolv run_test_sresolv

CLEANFILES =		resolv.conf error.conf named.conf.tmp \
			.test_sresolv.api.conf.?????? \
			named.run

LDADD =			libsresolv.la \
	 		../url/liburl.la ../bnf/libbnf.la \
			../su/libsu.la

test_sresolv_LDFLAGS =	-static

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = \
			sofia-sip/sresolv.h \
			sofia-sip/sres_sip.h \
			sofia-resolv/sres.h \
			sofia-resolv/sres_config.h \
			sofia-resolv/sres_async.h \
			sofia-resolv/sres_cache.h \
			sofia-resolv/sres_record.h

libsresolv_la_SOURCES =	sres.c sres_cache.c sres_blocking.c \
			sresolv.c sres_sip.c

COVERAGE_INPUT = 	$(libsresolv_la_SOURCES) $(include_sofia_HEADERS)

# ----------------------------------------------------------------------
# Distribution

EXTRA_DIST =		sresolv.docs

dist_noinst_DATA = run_test_sresolv \
	root.zone rndc.conf \
	127.zone 194.2.188 3.f.f.e.1.2.0.0.3.0.1.2.c.0.0.0 example.com \
	3.f.f.e.1.2.0.0.3.0.1.2.c.0.0.0.arpa

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

