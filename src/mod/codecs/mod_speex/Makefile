BASE=../../../..

SPEEX_DIR=$(BASE)/libs/speex
SPEEXLA=$(SPEEX_DIR)/libspeex/libspeex.la

LOCAL_CFLAGS=-I$(SPEEX_DIR)/include
LOCAL_LIBADD=$(SPEEXLA)
include $(BASE)/build/modmake.rules

$(SPEEXLA): $(SPEEX_DIR) $(SPEEX_DIR)/.update
	cd $(SPEEX_DIR)/libspeex && $(MAKE)
	$(TOUCH_TARGET)
