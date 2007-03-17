BASE=../../../..
GSM_DIR=$(BASE)/libs/codec/gsm
GSMLA=$(GSM_DIR)/libgsm.la
LOCAL_CFLAGS=-I$(GSM_DIR)/inc
LOCAL_LIBADD=$(GSMLA)
include $(BASE)/build/modmake.rules

$(GSMLA): $(GSM_DIR) $(GSM_DIR)/.update
	cd $(GSM_DIR) && $(MAKE)
	$(TOUCH_TARGET)
