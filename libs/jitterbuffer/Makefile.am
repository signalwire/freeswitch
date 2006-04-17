EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=make
NAME=jitterbuffer
PREFIX=$(prefix)
AM_CC=gcc
AM_CFLAGS	= -Wall -Werror -I./src -I$(prefix)/include
AM_CPPFLAGS = $(AM_CFLAGS)

AM_CFLAGS  += -I. -Isrc

lib_LTLIBRARIES		= libjitterbuffer.la
libjitterbuffer_la_SOURCES	= src/jitterbuffer.c
libjitterbuffer_la_CFLAGS	= $(AM_CFLAGS)
libjitterbuffer_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS = src/jitterbuffer.h

