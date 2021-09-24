#
# Makefile.am for http module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 	-I$(srcdir)/../bnf -I../bnf \
		-I$(srcdir)/../msg -I../msg \
		-I$(srcdir)/../url -I../url \
		-I$(srcdir)/../su -I../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libhttp.la

check_PROGRAMS = 	test_http

TESTS = 		test_http

# ----------------------------------------------------------------------
# Rules for building the targets

PUBLIC_H = 		sofia-sip/http.h sofia-sip/http_header.h \
			sofia-sip/http_parser.h sofia-sip/http_tag_class.h \
			sofia-sip/http_status.h sofia-sip/http_hclasses.h

BUILT_H = 		sofia-sip/http_protos.h sofia-sip/http_tag.h
BUILT_C = 		http_tag.c http_parser_table.c

BUILT_SOURCES = 	$(BUILT_H) $(BUILT_C) http_tag_ref.c

nobase_include_sofia_HEADERS = $(BUILT_H) $(PUBLIC_H)

libhttp_la_SOURCES = 	$(INTERNAL_H) \
			http_parser.c http_header.c \
			http_basic.c http_extra.c http_inlined.c \
			http_status.c http_tag_class.c \
			$(BUILT_SOURCES)

COVERAGE_INPUT = 	$(libhttp_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libhttp.la \
			../bnf/libbnf.la \
			../msg/libmsg.la \
			../url/liburl.la \
			../ipt/libipt.la \
			../su/libsu.la

test_http_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

EXTRA_DIST =		http.docs \
			sofia-sip/http_protos.h.in \
			sofia-sip/http_tag.h.in \
			http_parser_table.c.in \
			http_tag.c.in

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

TAG_DLL_FLAGS = DLLREF=1

MSG_PARSER_AWK = ${srcdir}/../msg/msg_parser.awk

AWK_HTTP_AWK = LC_ALL=C ${AWK} -f ${MSG_PARSER_AWK} module=http

SS_HTTP_H = ${srcdir}/sofia-sip/http.h

${BUILT_H} ${BUILT_C}: ${srcdir}/sofia-sip/http.h ${MSG_PARSER_AWK}

sofia-sip/http_protos.h: ${srcdir}/sofia-sip/http_protos.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	${AWK_HTTP_AWK} PR=$@ TEMPLATE=${srcdir}/sofia-sip/http_protos.h.in ${SS_HTTP_H}

sofia-sip/http_tag.h: ${srcdir}/sofia-sip/http_tag.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	${AWK_HTTP_AWK} PR=$@ TEMPLATE=${srcdir}/sofia-sip/http_tag.h.in ${SS_HTTP_H}

http_tag.c: ${srcdir}/http_tag.c.in
	${AWK_HTTP_AWK} PR=$@ TEMPLATE=${srcdir}/http_tag.c.in ${SS_HTTP_H}

http_parser_table.c: ${srcdir}/http_parser_table.c.in
	${AWK_HTTP_AWK} PT=$@ TEMPLATE=${srcdir}/http_parser_table.c.in \
		MC_HASH_SIZE=127 ${SS_HTTP_H}
