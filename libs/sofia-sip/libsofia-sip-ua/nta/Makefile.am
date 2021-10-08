#
# Makefile.am for nta module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 		-I$(srcdir)/../ipt -I../ipt \
			-I$(srcdir)/../msg -I../msg \
			-I$(srcdir)/../sip -I../sip \
			-I$(srcdir)/../bnf -I../bnf \
			-I$(srcdir)/../sresolv -I../sresolv \
			-I$(srcdir)/../tport -I../tport \
			-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../features -I../features \
			-I$(srcdir)/../su -I../su \
			-I$(top_srcdir)/s2check \
			-I$(srcdir)/../stun -I ../stun

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libnta.la

check_PROGRAMS = 	check_nta test_nta_api test_nta portbind
dist_noinst_SCRIPTS =	run_test_nta_api run_test_nta

TESTS =			run_check_nta run_test_nta_api run_test_nta

TESTS_ENVIRONMENT =	$(SHELL)

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		nta_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/nta.h sofia-sip/nta_stateless.h \
			sofia-sip/nta_tport.h sofia-sip/nta_tag.h \
			sofia-sip/sl_utils.h

libnta_la_SOURCES = 	nta.c nta_check.c nta_tag.c nta_tag_ref.c \
			nta_internal.h \
			sl_utils_print.c sl_utils_log.c \
			sl_read_payload.c

COVERAGE_INPUT = 	$(libnta_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libnta.la \
			../ipt/libipt.la \
			../sip/libsip.la \
			../features/libfeatures.la \
			../sresolv/libsresolv.la \
			../tport/libtport.la \
			../http/libhttp.la \
			../stun/libstun.la \
			../url/liburl.la \
			../msg/libmsg.la \
			../bnf/libbnf.la \
			../su/libsu.la \
			${top_builddir}/s2check/libs2.a

test_nta_LDFLAGS = 	-static

MOSTLYCLEANFILES +=	.test[0-9]*

if HAVE_CHECK

check_nta_SOURCES =	check_nta.c check_nta.h \
			check_nta_api.c \
			check_nta_client.c

check_nta_LDADD = 	${LDADD} @CHECK_LIBS@

else
check_nta_SOURCES = 	exit77.c
endif

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		nta.docs sl_utils.docs \
			agent.pem cafile.pem \
			invite.msc $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

# Generate list of nta tags
TAG_DLL_FLAGS = 	LIST=nta_tag_list
