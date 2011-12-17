EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTIONS = gnu
NAME=teletone

AM_CFLAGS	= $(new_AM_CFLAGS) -I./src
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS = -lm

lib_LTLIBRARIES		= libteletone.la
libteletone_la_SOURCES	= src/libteletone_generate.c src/libteletone_detect.c
libteletone_la_CFLAGS	= $(AM_CFLAGS)
libteletone_la_LDFLAGS	=  -version-info 0:1:0

library_includedir	= $(prefix)/include
library_include_HEADERS = src/libteletone.h src/libteletone_detect.h src/libteletone_generate.h



dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

