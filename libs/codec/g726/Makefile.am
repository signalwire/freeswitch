EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=gmake
NAME=libg726

AM_CC=gcc
AM_CFLAGS	= -Wall -O3 -funroll-loops
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libg726.la
libg726_la_SOURCES	= src/g726_16.c \
src/g726_24.c  \
src/g726_32.c  \
src/g726_40.c  \
src/g72x.c

libg726_la_CFLAGS	= $(AM_CFLAGS)
libg726_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS =  src/g72x.h src/private.h






