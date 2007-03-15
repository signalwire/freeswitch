# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:
BASE=../../../..
OSARCH=$(shell uname -s)
# and define these variables to impact your build
MODNAME=mod_portaudio
LOCAL_CFLAGS=-I. -I$(BASE)/libs/portaudio/include
LOCAL_LDFLAGS=$(BASE)/libs/portaudio/lib/libportaudio.la
ifeq ($(OSARCH),Darwin)
LOCAL_LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif
LOCAL_OBJS=pablio.o pa_ringbuffer.o

include ../../../../build/modmake.rules

local_depend:
	cd $(BASE)/libs/portaudio && $(MAKE)

pa_ringbuffer.o: pa_ringbuffer.c
	$(COMPILE) -D__EXTENSION__=1  -c -o $@ $<
