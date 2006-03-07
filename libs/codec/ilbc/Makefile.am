EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=gmake
NAME=libilbc

AM_CC=gcc
AM_CFLAGS	= -Wall -O3 -funroll-loops
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libilbc.la
libilbc_la_SOURCES	= src/anaFilter.c\
src/iCBSearch.c \
src/packing.c \
src/constants.c \
src/gainquant.c \
src/iLBC_decode.c \
src/StateConstructW.c \
src/createCB.c \
src/getCBvec.c \
src/iLBC_encode.c \
src/StateSearchW.c \
src/doCPLC.c \
src/helpfun.c \
src/syntFilter.c \
src/enhancer.c \
src/hpInput.c \
src/LPCdecode.c \
src/filter.c \
src/hpOutput.c \
src/LPCencode.c \
src/FrameClassify.c \
src/iCBConstruct.c \
src/lsf.c

libilbc_la_CFLAGS	= $(AM_CFLAGS)
libilbc_la_LDFLAGS	= 


library_includedir	= $(prefix)/include
library_include_HEADERS = src/iLBC_decode.h src/iLBC_encode.h 






