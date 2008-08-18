_DIST =
SUBDIRS =
AUTOMAKE_OPTS= gnu
NAME=resample

AM_CFLAGS       = $(new_AM_CFLAGS) -I./src
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS = $(new_AM_LDFLAGS) -lm

lib_LTLIBRARIES         = libresample.la
libresample_la_SOURCES  = src/resample.c src/resamplesubs.c src/filterkit.c
libresample_la_CFLAGS   = $(AM_CFLAGS)
libresample_la_LDFLAGS  = -version-info 1:3:1

library_includedir      = $(prefix)/include
library_include_HEADERS = include/libresample.h

bin_PROGRAMS = testresample
testresample_SOURCES = tests/testresample.c
testresample_CFLAGS = $(AM_CFLAGS)
testresample_LDADD = ${top_builddir}/libresample.la

#if SAMPLE
#bin_PROGRAMS += compareresample
#compareresample_SOURCES = tests/compareresample.c
#compareresample_CFLAGS = $(AM_CFLAGS)
#compareresample_LDADD = ${top_builddir}/libresample.la -lsamplerate
#endif

#if SNDFILE
#bin_PROGRAMS += resample_sndfile
#resample_sndfile_SOURCES = tests/resample-sndfile.c
#resample_sndfile_CFLAGS = $(AM_CFLAGS)
#resample_sndfile_LDADD = ${top_builddir}/libresample.la -lsndfile
#endif
