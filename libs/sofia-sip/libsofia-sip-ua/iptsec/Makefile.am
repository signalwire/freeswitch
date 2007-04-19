#
# Makefile.am for iptsec module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../bnf -I../bnf \
	 		-I$(srcdir)/../ipt -I../ipt \
	 		-I$(srcdir)/../http -I../http \
			-I$(srcdir)/../msg -I../msg \
			-I$(srcdir)/../nta -I../nta \
			-I$(srcdir)/../sip -I../sip \
			-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libiptsec.la

check_PROGRAMS = 	test_auth_digest

TESTS = 		test_auth_digest

# ----------------------------------------------------------------------
# Rules for building the targets

BUILT_SOURCES = 	auth_tag_ref.c

nobase_include_sofia_HEADERS = \
			sofia-sip/auth_common.h \
			sofia-sip/auth_client.h sofia-sip/auth_digest.h \
			sofia-sip/auth_module.h sofia-sip/auth_plugin.h \
			sofia-sip/auth_client_plugin.h

libiptsec_la_SOURCES = 	iptsec_debug.h \
			auth_client.c auth_common.c auth_digest.c \
			auth_module.c auth_tag.c auth_tag_ref.c \
			auth_plugin.c auth_plugin_delayed.c \
			auth_module_sip.c \
			iptsec_debug.c

COVERAGE_INPUT = 	$(libiptsec_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libiptsec.la \
			../nta/libnta.la \
			../sip/libsip.la \
			../msg/libmsg.la \
			../url/liburl.la \
			../bnf/libbnf.la \
			../ipt/libipt.la \
			../su/libsu.la

test_auth_digest_LDFLAGS = -static

if HAVE_NTLM
nobase_include_sofia_HEADERS += $(NTLM_HEADER)
libiptsec_la_SOURCES += $(NTLM_SOURCE)
endif

if HAVE_NTH
libiptsec_la_SOURCES += $(HTTP_SOURCE)
LDADD +=		../http/libhttp.la
endif

HTTP_SOURCE =		auth_module_http.c

NTLM_HEADER = 		sofia-sip/auth_ntlm.h
NTLM_SOURCE = 		auth_ntlm.c auth_client_ntlm.c auth_plugin_ntlm.c

EXTRA_libiptsec_la_SOURCES = \
			$(NTLM_HEADER) $(NTLM_SOURCE) $(HTTP_SOURCE)

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST = 		iptsec.docs testpasswd $(BUILT_SOURCES)

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

