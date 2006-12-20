##
## Process this file with automake to produce Makefile.in
##

INCLUDES = -I$(top_srcdir)/include

bin_PROGRAMS = ikslint iksroster iksperf

noinst_HEADERS = perf.h

ikslint_LDADD = $(top_builddir)/src/libiksemel.la
ikslint_SOURCES = ikslint.c hash.c

iksroster_LDADD = $(top_builddir)/src/libiksemel.la
iksroster_SOURCES = iksroster.c

iksperf_LDADD = $(top_builddir)/src/libiksemel.la
iksperf_SOURCES = iksperf.c perf.c
