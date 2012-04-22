#
# Asterisk -- A telephony toolkit for Linux.
# 
# Makefile for channel drivers
#
# Copyright (C) 1999-2005, Mark Spencer
#
# Mark Spencer <markster@digium.com>
#
# Edited By Belgarath <> Aug 28 2004
# Added bare bones ultrasparc-linux support.
#
# This program is free software, distributed under the terms of
# the GNU General Public License
#

#ASTERISK INCLUDE FILES
#The directory that contains the Asterisk include files (eg: /usr/include or /usr/include/asterisk or /usr/src/asterisk/include or ...)
#AST_INCLUDE_DIR=/usr/src/asterisk/include
#AST_INCLUDE_DIR=/home/maruzz/devel/svn_asterisk_trunk/include
#AST_INCLUDE_DIR=/home/maruzz/devel/svn_asterisk_branches_160/include
#AST_INCLUDE_DIR=/home/maruzz/devel/svn_asterisk_branches_12/include
#AST_INCLUDE_DIR=/home/maruzz/devel/svn_asterisk_branches_14/include
#AST_INCLUDE_DIR=/home/maruzz/devel/svn_celliax_trunk/asterisk-1.2.rev137401/include
AST_INCLUDE_DIR=/home/user/devel/asterisk-1.4.23.1/include

#ASTERISK
CFLAGS+=-DASTERISK

#ASTERISK VERSION
#Uncomment one of the following lines to match your Asterisk series
CFLAGS+=-DASTERISK_VERSION_1_4
#CFLAGS+=-DASTERISK_VERSION_1_6
#CFLAGS+=-DASTERISK_VERSION_1_2

#LINUX SKYPE SUPPORT (Celliax for Cygwin always supports Skype)
SKYPE_LIB=-L/usr/X11R6/lib -lX11

CFLAGS+=-pipe -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAGS+=-g3



CFLAGS+=-I$(AST_INCLUDE_DIR) -I.
CFLAGS+=-D_REENTRANT -D_GNU_SOURCE
#CFLAGS+=-O6
CFLAGS+=-march=i686
CFLAGS+=-fomit-frame-pointer
ifeq ($(shell uname -m),x86_64)
CFLAGS+=-fPIC
endif

SVNDEF := -D'SKYPIAX_SVN_VERSION="$(shell svnversion -n ..)"'
CFLAGS += $(SVNDEF)


SOLINK=-shared -Xlinker -x 
CHANNEL_LIBS=chan_skypiax.so
CC=gcc

OSARCH=$(shell uname -s)

ifeq ($(findstring CYGWIN,$(OSARCH)),CYGWIN)
# definition of pthread_kill as a printf (or as a noop) is required for Asterisk (and skypiax) to run on Cygwin
# without it, each time (often) pthread_kill is called (by any thread, with any signal, URG included), bad things happen 
CC=gcc -D pthread_kill=cyg_no_pthreadkill
AST_DLL_DIR=/home/maruzz/devel/svn_asterisk_branches_12
CYGSOLINK=-Wl,--out-implib=lib$@.a -Wl,--export-all-symbols cyg_no_pthread_kill.o
CYGSOLIB=-L/usr/lib/w32api -lrpcrt4 -L/lib/mingw -lwinmm -L$(AST_DLL_DIR) -lasterisk.dll -L$(AST_DLL_DIR)/res -lres_features.so 
SKYPE_LIB=
CHANNEL_LIBS=cyg_no_pthread_kill.o chan_skypiax.so
endif

all: $(CHANNEL_LIBS) 

clean:
	rm -f *.so *.o *.so.a


#chan_skypiax section begins

#to debug threads and lock on 1.4 uncomment the following
#CFLAGS+=-include /usr/src/asterisk/include/asterisk/autoconfig.h  


cyg_no_pthread_kill.o: cyg_no_pthread_kill.c
	$(CC) $(CFLAGS) -c -o cyg_no_pthread_kill.o cyg_no_pthread_kill.c
chan_skypiax.o: chan_skypiax.c
	$(CC) $(CFLAGS) -c -o chan_skypiax.o chan_skypiax.c
chan_skypiax.so: chan_skypiax.o skypiax_protocol.o
	$(CC) $(SOLINK) -o $@ ${CYGSOLINK} chan_skypiax.o skypiax_protocol.o -lm -ldl $(SKYPE_LIB) ${CYGSOLIB}
#chan_skypiax section ends

