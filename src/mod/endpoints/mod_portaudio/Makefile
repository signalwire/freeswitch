BASE=../../../..

PA_DIR=$(BASE)/libs/portaudio

PALA=$(PA_DIR)/lib/libportaudio.la
LOCAL_CFLAGS=-I. -I$(PA_DIR)/include

LOCAL_INSERT_LDFLAGS=if test $$osarch = "Darwin" ; then echo "-framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Carbon" ; fi ;

LOCAL_LIBADD=$(PALA)
LOCAL_OBJS=pablio.o pa_ringbuffer.o

include $(BASE)/build/modmake.rules

$(PALA): $(PA_DIR) $(PA_DIR)/.update
	cd $(PA_DIR) && $(MAKE)
	$(TOUCH_TARGET)

pa_ringbuffer.o: pa_ringbuffer.c
	$(COMPILE) -D__EXTENSION__=1  -c -o $@ $<

pablio.o: pablio.c
