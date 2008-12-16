#
# Makefile.am @template@ for tport module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 		-I$(srcdir)/../bnf -I../bnf \
			-I$(srcdir)/../stun -I../stun \
			-I$(srcdir)/../ipt -I../ipt \
			-I$(srcdir)/../msg -I../msg \
			-I$(srcdir)/../http -I../http \
			-I$(srcdir)/../url -I../url \
			-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libtport.la

bin_PROGRAMS =

check_PROGRAMS =	test_tport

TESTS =			test_tport

# ----------------------------------------------------------------------
# Rules for building the targets

nobase_include_sofia_HEADERS = \
			sofia-sip/tport.h sofia-sip/tport_tag.h \
			sofia-sip/tport_plugins.h

TLS_SRC =		tport_type_tls.c tport_tls.c tport_tls.h
if HAVE_TLS
USE_TLS_SRC = 		$(TLS_SRC)
endif

STUN_SRC =		tport_stub_stun.c tport_type_stun.c
if HAVE_STUN
USE_STUN_SRC = 		$(STUN_SRC)
endif

HTTP_SRC =		tport_type_connect.c
if HAVE_NTH
USE_HTTP_SRC = 		$(HTTP_SRC)
endif

libtport_la_SOURCES = 	tport.c tport_logging.c \
			tport_stub_sigcomp.c \
			tport_type_udp.c tport_type_tcp.c tport_type_sctp.c \
			tport_internal.h \
			tport_tag.c tport_tag_ref.c $(USE_HTTP_SRC) $(USE_TLS_SRC) $(USE_STUN_SRC)

# to make sure all files end up in the dist package
EXTRA_libtport_la_SOURCES = $(TLS_SRC) $(STUN_SRC) $(HTTP_SRC)

# Disable for now
EXTRA_libtport_la_SOURCES += tport_sigcomp.c tport_threadpool.c

BUILT_SOURCES =		tport_tag_ref.c

COVERAGE_INPUT = 	$(libtport_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libtport.la \
			../stun/libstun.la \
			../sresolv/libsresolv.la \
			../ipt/libipt.la \
			../http/libhttp.la \
			../msg/libtest_msg.a ../msg/libmsg.la \
			../url/liburl.la \
			../bnf/libbnf.la \
			../su/libsu.la

test_tport_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		tport.docs \
			certificates.html agent.pem cafile.pem

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

