BASE=../../../..
OSARCH=$(shell uname -s)

BASE=../../../..

PALA=$(BASE)/libs/portaudio/lib/libportaudio.la
LOCAL_CFLAGS=-I. -I$(BASE)/libs/portaudio/include
LOCAL_LDFLAGS=
ifeq ($(OSARCH),Darwin)
LOCAL_LDFLAGS += -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon
endif
LOCAL_LIBADD=$(PALA)
LOCAL_OBJS=pablio.o pa_ringbuffer.o

include $(BASE)/build/modmake.rules

$(PALA):
	cd $(BASE)/libs/portaudio && $(MAKE)

pa_ringbuffer.o: pa_ringbuffer.c
	$(COMPILE) -D__EXTENSION__=1  -c -o $@ $<
