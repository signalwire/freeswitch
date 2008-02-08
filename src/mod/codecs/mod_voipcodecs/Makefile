BASE=../../../..

VOIPCODECS_DIR=$(BASE)/libs/voipcodecs
VOIPCODECS_LA=$(VOIPCODECS_DIR)/src/libvoipcodecs.la

LOCAL_CFLAGS=-I$(VOIPCODECS_DIR)/src
LOCAL_LIBADD=$(VOIPCODECS_LA)

include $(BASE)/build/modmake.rules

$(VOIPCODECS_LA): $(VOIPCODECS_DIR) $(VOIPCODECS_DIR)/.update
	cd $(VOIPCODECS_DIR) && $(MAKE)
	$(TOUCH_TARGET)
