BASE=../../../..
LPC10_DIR=$(BASE)/libs/codec/lpc10
LPC10LA=$(LPC10_DIR)/liblpc10.la
LOCAL_CFLAGS=-I$(LPC10_DIR)/src
LOCAL_LIBADD=$(LPC10LA)
include $(BASE)/build/modmake.rules

$(LPC10LA): $(LPC10_DIR) $(LPC10_DIR)/.update
	cd $(LPC10_DIR) && $(MAKE)
