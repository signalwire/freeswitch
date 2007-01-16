EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=liblpc10

AM_CC=gcc
AM_CFLAGS	= -Wall -O3 -funroll-loops
AM_CPPFLAGS = $(AM_CFLAGS)

lib_LTLIBRARIES		= libg7xx.la
liblpc10_la_SOURCES = analys.c  dcbias.c  difmag.c  energy.c  hp100.c   ivfilt.c  lpcini.c  mload.c   placea.c  prepro.c  synths.c  vparms.c \
bsynz.c   decode.c  dyptrk.c  f2clib.c  invert.c  lpcdec.c  lpfilt.c  onset.c   placev.c  random.c  tbdm.c \
chanwr.c  deemp.c   encode.c  ham84.c   irc2pc.c  lpcenc.c  median.c  pitsyn.c  preemp.c  rcchk.c   voicin.c 

liblpc10_la_CFLAGS	= $(AM_CFLAGS)
liblpc10_la_LDFLAGS	= 

library_includedir	= $(prefix)/include/lpc10
library_include_HEADERS =  f2c.h  lpc10.h






