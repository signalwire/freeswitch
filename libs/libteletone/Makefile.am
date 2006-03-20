EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=make
NAME=teletone

AM_CC=gcc
AM_CFLAGS	= -D_GNU_SOURCE -Wall -Werror -ansi -pedantic -g -O2 -I./src
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS = -lm

lib_LTLIBRARIES		= libteletone.la
libteletone_la_SOURCES	= src/libteletone_generate.c src/libteletone_detect.c
libteletone_la_CFLAGS	= $(AM_CFLAGS)
libteletone_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS = src/libteletone.h src/libteletone_detect.h src/libteletone_generate.h



dox:
	cd docs && doxygen $(PWD)/docs/Doxygen.conf

