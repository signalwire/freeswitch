#
# Makefile.am for Sofia SIP tests
#
# Copyright (C) 2007,2008 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

TESTS = test_nua
check_PROGRAMS = test_nua

EXTRA_DIST = check_sofia.h check_sofia.c suite_for_nua.c

test_nua_SOURCES = 	test_nua.c

test_nua_LDADD =	$(check_LIBRARIES) \
			${sofiabuilddir}/libsofia-sip-ua.la

check_LIBRARIES =   	libtestnua.a libtestproxy.a libtestnat.a

libtestnua_a_SOURCES =	test_nua.h test_ops.c \
			test_init.c \
			test_nua_api.c test_nua_params.c \
			test_register.c test_basic_call.c \
			test_offer_answer.c \
			test_call_reject.c test_cancel_bye.c \
			test_call_hold.c test_session_timer.c \
			test_refer.c test_100rel.c \
			test_simple.c test_sip_events.c \
			test_extension.c

libtestproxy_a_SOURCES = test_proxy.h test_proxy.c

libtestnat_a_SOURCES =	test_nat.h test_nat.c test_nat_tags.c

if HAVE_CHECK
TESTS += check_dlopen_sofia check_sofia
check_PROGRAMS += check_dlopen_sofia check_sofia
endif

check_sofia_CFLAGS = @CHECK_CFLAGS@

check_sofia_SOURCES = check_sofia.c check_sofia.h \
	suite_for_nua.c

check_sofia_LDADD = 	$(check_LIBRARIES) \
			${sofiabuilddir}/libsofia-sip-ua.la \
			@CHECK_LIBS@

check_dlopen_sofia_CFLAGS = -I$(top_srcdir)/s2check @CHECK_CFLAGS@
check_dlopen_sofia_LDADD = ${top_builddir}/s2check/libs2.a @CHECK_LIBS@

CLEANFILES =            tmp_sippasswd.??????

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

INCLUDES = ${INTERNAL_INCLUDES}
