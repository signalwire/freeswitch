AM_CFLAGS	= -I./src/include -fPIC -O3 -Dsingle -funroll-loops -fomit-frame-pointer
AUTOMAKE_OPTS	= gnu
MAKE		= gmake
NAME		= libg7xx
lib_LTLIBRARIES		= libg7xx.la
libg7xx_la_SOURCES	= src/g711.c src/g722_decode.c src/g722_encode.c src/g726.c

libg7xx_la_CFLAGS	= $(AM_CFLAGS)
libg7xx_la_LDFLAGS	= 

library_includedir      = $(prefix)/include/g7xx
library_include_HEADERS = src/include/g722.h src/include/g711.h src/include/g726.h


