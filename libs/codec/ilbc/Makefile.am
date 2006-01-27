EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
MAKE=gmake
NAME=libilbc

AM_CC=gcc
AM_CFLAGS	= -Wall -O3 -funroll-loops
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libilbc.la
libgsm_la_SOURCES	= src/anaFilter.o src/iCBSearch.o src/packing.o \
        src/constants.o src/gainquant.o src/iLBC_decode.o src/StateConstructW.o \
        src/createCB.o src/getCBvec.o src/iLBC_encode.o src/StateSearchW.o src/doCPLC.o \
        src/helpfun.o src/syntFilter.o src/enhancer.o src/hpInput.o src/LPCdecode.o \
        src/filter.o src/hpOutput.o src/LPCencode.o src/FrameClassify.o  src/iCBConstruct.o src/lsf.o

libgsm_la_CFLAGS	= $(AM_CFLAGS)
libgsm_la_LDFLAGS	= 






