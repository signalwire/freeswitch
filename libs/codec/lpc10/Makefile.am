EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=libg7xx

AM_CC=gcc
AM_CFLAGS	= -Wall -O3 -funroll-loops
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libg7xx.la
libg7xx_la_SOURCES	= g711.c g722_deode.c  g722_encode.c  g726.c  

libg7xx_la_CFLAGS	= $(AM_CFLAGS)
libg7xx_la_LDFLAGS	= 

library_includedir	= $(prefix)/include/g7xx
library_include_HEADERS =  g726.h g711.h  g722.h






