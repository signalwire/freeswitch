#
# Makefile.am for sip module
#

# ----------------------------------------------------------------------
# Header paths

INCLUDES = 	-I$(srcdir)/../bnf -I../bnf \
		-I$(srcdir)/../ipt -I../ipt \
		-I$(srcdir)/../msg -I../msg \
		-I$(srcdir)/../url -I../url \
		-I$(srcdir)/../su -I../su

# ----------------------------------------------------------------------
# Build targets

noinst_LTLIBRARIES = 	libsip.la

check_PROGRAMS =       torture_sip \
			test_sip_msg validator test_date

# ----------------------------------------------------------------------
# Rules for building the targets

GENERATED_H = 		sofia-sip/sip_hclasses.h \
			sofia-sip/sip_protos.h sofia-sip/sip_tag.h \
			sofia-sip/sip_extra.h

H_IN = 			sofia-sip/sip_hclasses.h.in \
			sofia-sip/sip_protos.h.in sofia-sip/sip_tag.h.in \
			sofia-sip/sip_extra.h.in

PUBLIC_H = sofia-sip/sip.h sofia-sip/sip_util.h \
           sofia-sip/sip_header.h sofia-sip/sip_parser.h \
	   sofia-sip/sip_tag_class.h sofia-sip/sip_status.h

GENERATED_C = sip_tag.c sip_parser_table.c

BUILT_SOURCES = 	$(GENERATED_H) $(GENERATED_C) sip_tag_ref.c

nobase_include_sofia_HEADERS = $(GENERATED_H) $(PUBLIC_H) $(H_IN)

libsip_la_SOURCES = 	$(INTERNAL_H) \
			sip_parser.c sip_header.c sip_util.c sip_pref_util.c \
			sip_basic.c sip_extra.c sip_feature.c sip_mime.c \
			sip_security.c sip_event.c sip_prack.c \
			sip_refer.c sip_session.c \
			sip_caller_prefs.c sip_reason.c \
			sip_status.c sip_time.c \
			sip_tag_class.c sip_inlined.c \
			$(BUILT_SOURCES)

COVERAGE_INPUT = 	$(libsip_la_SOURCES) $(include_sofia_HEADERS)

LDADD = 		libsip.la \
			../msg/libmsg.la \
			../bnf/libbnf.la \
			../url/liburl.la \
			../ipt/libipt.la \
			../su/libsu.la

torture_sip_LDFLAGS = 	-static
test_sip_msg_LDFLAGS = 	-static
test_date_LDFLAGS = 	-static

# ----------------------------------------------------------------------
# Install and distribution rules

# note: srcdir needs to be specified, otherwise
#       breaks make distcheck target

EXTRA_DIST =		sip.docs sip_parser.docs sip.doxyaliases \
			ADD-A-HEADER GRAMMAR sip_bad_mask \
			sip_parser_table.c.in sip_tag.c.in \
			sip_extra_headers.txt \
	images/sip-parser.eps images/sip-parser.gif \
	images/sip-parser2.eps images/sip-parser2.gif \
	images/sip-parser3.eps images/sip-parser3.gif \
	images/sip-parser4.eps images/sip-parser4.gif \
	tests/own0.txt tests/own1.txt tests/own2.txt tests/own3.txt \
	tests/own4.txt tests/own5.txt tests/own6.txt tests/own8.txt \
	tests/test1.txt tests/test-ack-1.txt tests/test2.txt tests/test3.txt \
	tests/test4.txt tests/test5.txt tests/test6.txt tests/test7.txt	     \
	tests/test8.txt tests/test9.txt tests/test10.txt tests/test10b.txt   \
	tests/test10c.txt tests/test11.txt tests/test12.txt tests/test13.txt \
	tests/test14-req.txt tests/test14.txt tests/test15.txt		     \
	tests/test16.txt tests/test17.txt tests/test18.txt tests/test19.txt  \
	tests/test1a.txt tests/test20.txt tests/test21.txt tests/test22.txt  \
	tests/test23.txt tests/test24.txt tests/test25.txt tests/test26.txt  \
	tests/test27.txt tests/test28.txt tests/test29.txt tests/test30.txt  \
	tests/test31.txt tests/test32.txt tests/test33.txt tests/test34.txt  \
	tests/test35.txt tests/test36.txt tests/test37.txt tests/test38.txt  \
	tests/test39.txt tests/test40.txt tests/test41.txt tests/test42.txt

# ----------------------------------------------------------------------
# Tests

TESTS = torture_sip run_test_sip_msg run_test_date

dist_noinst_SCRIPTS = 	run_test_sip_msg run_test_date

# ----------------------------------------------------------------------
# Sofia specific rules

include $(top_srcdir)/rules/sofia.am

MSG_PARSER_AWK = $(srcdir)/../msg/msg_parser.awk

AWK_SIP_AWK = LC_ALL=C $(AWK) -f $(MSG_PARSER_AWK) module=sip

SS_SIP_H = ${srcdir}/sofia-sip/sip.h

EXTRA = ${srcdir}/sip_extra_headers.txt

${GENERATED_H} ${GENERATED_C}: ${SS_SIP_H} ${MSG_PARSER_AWK}

sofia-sip/sip_hclasses.h: ${srcdir}/sofia-sip/sip_hclasses.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	${AWK_SIP_AWK} PR=$@ TEMPLATE=${srcdir}/sofia-sip/sip_hclasses.h.in \
		${SS_SIP_H}

sofia-sip/sip_protos.h: ${srcdir}/sofia-sip/sip_protos.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	${AWK_SIP_AWK} PR=$@ TEMPLATE=${srcdir}/sofia-sip/sip_protos.h.in \
		 ${SS_SIP_H}

sofia-sip/sip_tag.h: ${srcdir}/sofia-sip/sip_tag.h.in
	@-mkdir sofia-sip 2>/dev/null || true
	${AWK_SIP_AWK} PR=$@ TEMPLATE=${srcdir}/sofia-sip/sip_tag.h.in \
		${SS_SIP_H}

sip_tag.c: ${srcdir}/sip_tag.c.in ${EXTRA}
	${AWK_SIP_AWK} PR=$@ TEMPLATE=${srcdir}/sip_tag.c.in \
		${SS_SIP_H} ${EXTRA}

# Note: sip_bad_mask is used by nta to weed out bad messages

sip_parser_table.c: ${srcdir}/sip_parser_table.c.in ${EXTRA} sip_bad_mask
	${AWK_SIP_AWK} PT=$@ TEMPLATE=${srcdir}/sip_parser_table.c.in \
		FLAGFILE=${srcdir}/sip_bad_mask \
		MC_HASH_SIZE=127 MC_SHORT_SIZE=26 \
		${SS_SIP_H} ${EXTRA}

sofia-sip/sip_extra.h: ${srcdir}/sofia-sip/sip_extra.h.in ${EXTRA}
	@-mkdir -p sofia-sip 2>/dev/null
	${AWK_SIP_AWK} PR=$@ NO_FIRST=1 NO_LAST=1 \
		PACKAGE_NAME="${PACKAGE_NAME}" \
		PACKAGE_VERSION="${PACKAGE_VERSION}" \
		TEMPLATE1=${srcdir}/sofia-sip/sip_hclasses.h.in \
		TEMPLATE2=${srcdir}/sofia-sip/sip_protos.h.in \
		TEMPLATE3=${srcdir}/sofia-sip/sip_tag.h.in \
		TEMPLATE=${srcdir}/sofia-sip/sip_extra.h.in ${EXTRA}
