MODNAME=mod_gsmopen
SVNDEF := -D'GSMOPEN_SVN_VERSION="$(shell svnversion -n .)"'
LOCAL_CFLAGS += $(SVNDEF) -DNO_ALSA -DGSMOPEN_PORTAUDIO -I../../../../../../libs/spandsp/src -I../../../../../..//libs/tiff-3.8.2/libtiff
LOCAL_LDFLAGS=-lportaudio -lspeex -lspeexdsp -lgsmme -L../../../../../../libs/spandsp/src -lspandsp
LOCAL_OBJS=gsmopen_protocol.o pablio.o pa_ringbuffer.o 
include ../../../../../../build/modmake.rules
