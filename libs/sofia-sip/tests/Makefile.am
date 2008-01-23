#
# Tests using check
#

EXTRA_DIST = check_sofia.h check_sofia.c suite_for_nua.c

TESTS =
check_PROGRAMS =

include $(top_srcdir)/rules/sofia.am

INCLUDES = ${INTERNAL_INCLUDES}

AM_CFLAGS += @CHECK_CFLAGS@

if HAVE_CHECK
TESTS += check_sofia
check_PROGRAMS += check_sofia

check_sofia_SOURCES = check_sofia.c check_sofia.h \
	suite_for_nua.c

check_sofia_LDADD = \
	${sofiabuilddir}/nua/libtestnua.la \
	${sofiabuilddir}/nua/libtestproxy.la \
	${sofiabuilddir}/nua/libtestnat.la \
	${sofiabuilddir}/libsofia-sip-ua.la @CHECK_LIBS@
endif
