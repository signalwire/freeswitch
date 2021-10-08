#
# Makefile.am for msg module
#
# Copyright (C) 2005,2006 Nokia Corporation
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
# Licensed under LGPL. See file COPYING.

# ----------------------------------------------------------------------
# Header paths

AM_CPPFLAGS = 	-I$(srcdir)/../url -I../url \
		-I$(srcdir)/../bnf -I../bnf \
		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libmsg.la
noinst_LIBRARIES = 	libtest_msg.a
check_PROGRAMS = 	msg_name_hash test_msg

# ----------------------------------------------------------------------
# Rules for building the targets

GENERATED_H = 		sofia-sip/msg_protos.h sofia-sip/msg_mime_protos.h
PUBLIC_H = 		sofia-sip/msg.h sofia-sip/msg_header.h \
			sofia-sip/msg_types.h sofia-sip/msg_mclass.h \
			sofia-sip/msg_mclass_hash.h sofia-sip/msg_parser.h \
			sofia-sip/msg_addr.h sofia-sip/msg_date.h \
			sofia-sip/msg_buffer.h sofia-sip/msg_tag_class.h \
			sofia-sip/msg_mime.h

INTERNAL_H = msg_internal.h test_class.h

nobase_include_sofia_HEADERS = \
			$(GENERATED_H) $(PUBLIC_H)

GENERATED_HC = $(GENERATED_H) msg_mime_table.c test_table.c test_protos.h

BUILT_SOURCES = $(GENERATED_HC)

libmsg_la_SOURCES = msg_internal.h \
	msg.c msg_tag.c msg_inlined.c \
	msg_mime.c msg_mime_table.c \
	msg_header_copy.c msg_header_make.c \
	msg_parser.c msg_mclass.c msg_parser_util.c \
	msg_basic.c msg_generic.c msg_date.c msg_auth.c

COVERAGE_INPUT = 	$(libmsg_la_SOURCES) $(include_sofia_HEADERS)

libtest_msg_a_SOURCES = test_class.c test_class.h \
			test_table.c test_inlined.c test_protos.h

LDADD = 		libtest_msg.a libmsg.la \
			../bnf/libbnf.la \
			../url/liburl.la \
			../ipt/libipt.la \
			../su/libsu.la

test_msg_LDFLAGS = 	-static

msg_name_hash_LDFLAGS =	-static

# ----------------------------------------------------------------------
# Install and distribution rules

dist_pkgdata_SCRIPTS = 	msg_parser.awk

EXTRA_DIST =		msg.docs \
			sofia-sip/msg_mime_protos.h.in \
			sofia-sip/msg_protos.h.in \
			msg_mime_table.c.in \
			test_protos.h.in \
			test_table.c.in

# ----------------------------------------------------------------------
# Tests

TESTS = 		test_msg

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

MSG_PARSER_AWK = $(srcdir)/msg_parser.awk

AWK_MSG_AWK = LC_ALL=C $(AWK) -f $(MSG_PARSER_AWK)

${GENERATED_HC}: ${MSG_PARSER_AWK} Makefile.am

TEST_CLASS_H = ${srcdir}/test_class.h

test_protos.h test_table.c: ${TEST_CLASS_H}

test_protos.h: ${srcdir}/test_protos.h.in
	$(AWK_MSG_AWK) module=msg_test NO_MIDDLE=1 NO_LAST=1 \
		PR=$@ TEMPLATE=${srcdir}/test_protos.h.in ${TEST_CLASS_H}

test_table.c: ${srcdir}/test_table.c.in
	$(AWK_MSG_AWK) module=msg_test prefix=msg \
		MC_HASH_SIZE=127 multipart=msg_multipart \
		PT=$@ TEMPLATE=${srcdir}/test_table.c.in ${TEST_CLASS_H}

SS_MIME_H = ${srcdir}/sofia-sip/msg_mime.h

sofia-sip/msg_protos.h sofia-sip/msg_mime_protos.h: ${SS_MIME_H}
msg_mime_table.c: ${SS_MIME_H}

sofia-sip/msg_protos.h: ${srcdir}/sofia-sip/msg_protos.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	$(AWK_MSG_AWK) module=msg NO_FIRST=1 NO_MIDDLE=1 \
		PR=$@ TEMPLATE=${srcdir}/sofia-sip/msg_protos.h.in \
		${SS_MIME_H}

sofia-sip/msg_mime_protos.h: ${srcdir}/sofia-sip/msg_mime_protos.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	$(AWK_MSG_AWK) module=msg NO_FIRST=1 NO_LAST=1 \
		PR=$@ TEMPLATE=${srcdir}/sofia-sip/msg_mime_protos.h.in \
		${SS_MIME_H}

msg_mime_table.c: ${srcdir}/msg_mime_table.c.in
	$(AWK_MSG_AWK) module=msg_multipart \
		tprefix=msg prefix=mp MC_HASH_SIZE=127 MC_SHORT_SIZE=26 \
		PT=$@ TEMPLATE=${srcdir}/msg_mime_table.c.in \
		${SS_MIME_H}
