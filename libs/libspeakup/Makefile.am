EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=speakup
PREFIX=$(prefix)
AM_CFLAGS	= $(new_AM_CFLAGS) -I. -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libspeakup.la
libspeakup_la_SOURCES	= src/jb_speakup.c
libspeakup_la_CFLAGS	= $(AM_CFLAGS)
libspeakup_la_LDFLAGS	=  -version-info 1:0:1

library_includedir	= $(prefix)/include
library_include_HEADERS = src/jb_speakup.h
