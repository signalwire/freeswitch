#
# Makefile.am for nua module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../bnf -I../bnf \
	 		-I$(srcdir)/../ipt -I../ipt \
	 		-I$(srcdir)/../iptsec -I../iptsec \
	 		-I$(srcdir)/../http -I../http \
			-I$(srcdir)/../msg -I../msg \
			-I$(srcdir)/../nea -I../nea \
			-I$(srcdir)/../nta -I../nta \
			-I$(srcdir)/../nth -I../nth \
			-I$(srcdir)/../sdp -I../sdp \
			-I$(srcdir)/../sip -I../sip \
			-I$(srcdir)/../soa -I../soa \
			-I$(srcdir)/../tport -I../tport \
			-I$(srcdir)/../stun -I../stun \
			-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libnua.la

check_PROGRAMS =	test_nua

TESTS = 		test_nua

CLEANFILES =           tmp_sippasswd.??????

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		nua_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/nua.h sofia-sip/nua_tag.h

libnua_la_SOURCES = 	nua.c nua_stack.h nua_common.c nua_stack.c \
			nua_extension.c \
			nua_dialog.c nua_dialog.h \
			outbound.c outbound.h \
			nua_params.c nua_params.h \
			nua_register.c nua_registrar.c \
			nua_session.c nua_options.c \
			nua_message.c nua_publish.c nua_subnotref.c \
			nua_notifier.c \
			nua_event_server.c \
			nua_tag.c nua_tag_ref.c

COVERAGE_INPUT = 	$(libnua_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libnua.la \
			../iptsec/libiptsec.la \
			../ipt/libipt.la \
			../nea/libnea.la \
			../nta/libnta.la \
			../sresolv/libsresolv.la \
			../tport/libtport.la \
			../stun/libstun.la \
			../soa/libsoa.la \
			../sdp/libsdp.la \
			../sip/libsip.la \
			../http/libhttp.la \
			../msg/libmsg.la \
			../url/liburl.la \
			../bnf/libbnf.la \
			../su/libsu.la

test_nua_LDFLAGS = 	-static

test_nua_SOURCES = 	test_nua.c test_nua.h test_ops.c \
			test_init.c \
			test_nua_api.c test_nua_params.c \
			test_register.c test_basic_call.c \
			test_offer_answer.c \
			test_call_reject.c test_cancel_bye.c \
			test_call_hold.c test_session_timer.c \
			test_refer.c test_100rel.c \
			test_simple.c test_sip_events.c \
			test_extension.c \
			test_proxy.h test_proxy.c \
			test_nat.h test_nat.c test_nat_tags.c

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		nua.docs $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

TAG_DLL_FLAGS =		LIST=nua_tag_list
