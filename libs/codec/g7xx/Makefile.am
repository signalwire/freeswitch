AM_CFLAGS	= -I./src/include -fPIC -O3 -Dsingle -funroll-loops -fomit-frame-pointer
AUTOMAKE_OPTS	= gnu
MAKE		= gmake
NAME		= libg7xx
lib_LTLIBRARIES		= libg7xx.la
libg7xx_la_SOURCES	= g711.c g722_decode.c g722_encode.c g726.c

libg7xx_la_CFLAGS	= $(AM_CFLAGS)
libg7xx_la_LDFLAGS	= 

library_includedir      = $(prefix)/include/g7xx
library_include_HEADERS = g722.h g711.h g726.h


