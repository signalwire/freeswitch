#
# Makefile.am for nua module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libnua.la

check_PROGRAMS =
TESTS =

if HAVE_CHECK
check_PROGRAMS +=	check_nua
TESTS += 		check_nua
endif

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES =		nua_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/nua.h sofia-sip/nua_tag.h

libnua_la_SOURCES = 	nua.c nua_stack.h nua_common.c nua_stack.c \
			nua_server.h nua_server.c \
			nua_client.h nua_client.c \
			nua_extension.c nua_types.h \
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

# ----------------------------------------------------------------------

check_nua_SOURCES = 	check_nua.c check_nua.h \
			check_session.c check_register.c \
			check_etsi.c check_simple.c

check_nua_LDADD = 	$(nua_libs) ${top_builddir}/s2check/libs2.a \
			@CHECK_LIBS@

nua_libs = 		libnua.la  \
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

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		nua.docs $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

INCLUDES = ${INTERNAL_INCLUDES} -I$(top_srcdir)/s2check

TAG_DLL_FLAGS =		LIST=nua_tag_list
