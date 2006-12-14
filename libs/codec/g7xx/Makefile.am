AUTOMAKE_OPTS	= gnu
NAME		= libg7xx

AM_CFLAGS   = $(new_AM_CFLAGS) -I./src/include
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS) -lm

lib_LTLIBRARIES		= libg7xx.la
libg7xx_la_SOURCES	= src/g711.c src/g722_decode.c src/g722_encode.c src/g726.c

libg7xx_la_CFLAGS	= $(AM_CFLAGS)
libg7xx_la_LDFLAGS	= 

library_includedir      = $(prefix)/include/g7xx
library_include_HEADERS = src/include/g7xx/g722.h src/include/g7xx/g711.h src/include/g7xx/g726.h


