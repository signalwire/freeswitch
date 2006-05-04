EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=make
NAME=speakup
PREFIX=$(prefix)
AM_CC=gcc
AM_CFLAGS	= -Wall -Werror -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)

AM_CFLAGS  += -I. -Isrc

lib_LTLIBRARIES		= libspeakup.la
libspeakup_la_SOURCES	= src/jb_speakup.c
libspeakup_la_CFLAGS	= $(AM_CFLAGS)
libspeakup_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS = src/jb_speakup.h

