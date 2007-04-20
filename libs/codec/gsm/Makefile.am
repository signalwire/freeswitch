EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=libgsm

AM_CC=gcc
AM_CFLAGS	=  -I./inc -DNeedFunctionPrototypes=1   -fPIC -DSASR -DNDEBUG -DWAV49
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libgsm.la
libgsm_la_SOURCES	= src/add.c\
src/code.c\
src/decode.c\
src/gsm_create.c\
src/gsm_decode.c\
src/gsm_destroy.c\
src/gsm_encode.c\
src/gsm_lpc.c\
src/gsm_option.c\
src/long_term.c\
src/preprocess.c\
src/rpe.c\
src/short_term.c\
src/table.c

libgsm_la_CFLAGS	= $(AM_CFLAGS)
libgsm_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS = inc/config.h\
inc/gsm.h\
inc/private.h\
inc/proto.h\
inc/unproto.h








