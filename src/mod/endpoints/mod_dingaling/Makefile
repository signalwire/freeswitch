BASE=../../../..

DING_DIR=$(BASE)/libs/libdingaling
DINGLA=$(DING_DIR)/libdingaling.la

LOCAL_CFLAGS=-I$(DING_DIR)/src
LOCAL_LIBADD=$(DINGLA)

include $(BASE)/build/modmake.rules

$(DINGLA): $(DING_DIR) $(DING_DIR)/.update
	cd $(DING_DIR) && $(MAKE)
	$(TOUCH_TARGET)
