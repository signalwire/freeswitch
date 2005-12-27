# 
# Makefile for LPC-10 speech coder library (unix)
# 

# default C compiler
CC= gcc

# 
# These definitions for CFLAGS and LIB_TARGET_DIR are used when one
# runs make in the lpc10 directory, without environment variables that
# override them.  When make is run in this directory from a makefile
# for an application that uses the LPC10 coder, there are environment
# variables set for CFLAGS and LIB_TARGET_DIR that override these
# definitions.
# 

LIB_TARGET_DIR = ..

# 
# -I$(LIB_TARGET_DIR) option needed so that #include "machine.h"
# directives can find the machine.h file.
# 

WARNINGS = -Wall
CFLAGS = -O3 -I$(LIB_TARGET_DIR) $(WARNINGS)

LIB = $(LIB_TARGET_DIR)/liblpc10.a

.PHONY: all clean

all:	$(LIB)
	ranlib $(LIB)

$(LIB):		$(LIB)(f2clib.o)	\
		$(LIB)(analys.o)	\
		$(LIB)(bsynz.o)		\
		$(LIB)(chanwr.o)	\
		$(LIB)(dcbias.o)	\
		$(LIB)(decode.o)	\
		$(LIB)(deemp.o)		\
		$(LIB)(difmag.o)	\
		$(LIB)(dyptrk.o)	\
		$(LIB)(encode.o)	\
		$(LIB)(energy.o)	\
		$(LIB)(ham84.o)		\
		$(LIB)(hp100.o)		\
		$(LIB)(invert.o)	\
		$(LIB)(irc2pc.o)	\
		$(LIB)(ivfilt.o)	\
		$(LIB)(lpcdec.o)	\
		$(LIB)(lpcenc.o)	\
		$(LIB)(lpcini.o)	\
		$(LIB)(lpfilt.o)	\
		$(LIB)(median.o)	\
		$(LIB)(mload.o)		\
		$(LIB)(onset.o)		\
		$(LIB)(pitsyn.o)	\
		$(LIB)(placea.o)	\
		$(LIB)(placev.o)	\
		$(LIB)(preemp.o)	\
		$(LIB)(prepro.o)	\
		$(LIB)(random.o)	\
		$(LIB)(rcchk.o)		\
		$(LIB)(synths.o)	\
		$(LIB)(tbdm.o)		\
		$(LIB)(voicin.o)	\
		$(LIB)(vparms.o)

clean:
	-rm -f *.o
