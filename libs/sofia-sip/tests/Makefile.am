#
# Tests using check
#

EXTRA_DIST = check_sofia.h check_sofia.c suite_for_nua.c

TESTS =
tests_PROGRAMS =
testsdir := ${libdir}/tests/libsofia-sip-ua-tests

include $(top_srcdir)/rules/sofia.am

INCLUDES = ${INTERNAL_INCLUDES}

AM_CFLAGS += @CHECK_CFLAGS@

if HAVE_CHECK

TESTS += check_sofia
tests_PROGRAMS += check_sofia

check_sofia_SOURCES = check_sofia.c check_sofia.h \
	suite_for_nua.c

CHECKLIBS = ${sofiabuilddir}/nua/libtestnua.la \
	${sofiabuilddir}/nua/libtestproxy.la \
	${sofiabuilddir}/nua/libtestnat.la

$(CHECKLIBS):
	cd ${sofiabuilddir}/nua && make checklib

check_sofia_LDADD = \
	$(CHECKLIBS) ${sofiabuilddir}/libsofia-sip-ua.la \
	@CHECK_LIBS@
endif
