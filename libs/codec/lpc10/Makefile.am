EXTRA_DIST =
SUBDIRS = 
AUTOMAKE_OPTS= gnu
NAME=liblpc10

AM_CFLAGS   = $(new_AM_CFLAGS)
AM_CPPFLAGS = $(AM_CFLAGS)
AM_LDFLAGS  = $(new_AM_LDFLAGS) -lm

lib_LTLIBRARIES		= liblpc10.la
liblpc10_la_SOURCES = src/analys.c src/dcbias.c src/difmag.c src/energy.c src/hp100.c \
src/ivfilt.c src/lpcini.c src/mload.c src/placea.c src/prepro.c src/synths.c \
src/vparms.c src/bsynz.c src/decode.c src/dyptrk.c src/f2clib.c src/invert.c \
src/lpcdec.c src/lpfilt.c src/onset.c src/placev.c src/random.c src/tbdm.c \
src/chanwr.c src/deemp.c src/encode.c src/ham84.c src/irc2pc.c src/lpcenc.c \
src/median.c src/pitsyn.c src/preemp.c src/rcchk.c src/voicin.c 

liblpc10_la_CFLAGS	= $(AM_CFLAGS)
liblpc10_la_LDFLAGS	= 

library_includedir	= $(prefix)/include
library_include_HEADERS =  src/f2c.h  src/lpc10.h






