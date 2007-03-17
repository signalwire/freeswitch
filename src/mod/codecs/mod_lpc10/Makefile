
BASE=../../../..
LPC10LA=$(BASE)/libs/codec/lpc10/liblpc10.la
LOCAL_CFLAGS=-I$(BASE)/libs/codec/lpc10/src
LOCAL_LIBADD=$(LPC10LA)
include $(BASE)/build/modmake.rules

$(LPC10LA):
	cd $(BASE)/libs/codec/lpc10 && $(MAKE)
