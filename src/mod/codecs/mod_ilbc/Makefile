BASE=../../../..
ILBC_DIR=$(BASE)/libs/codec/ilbc
ILBCLA=$(ILBC_DIR)/libilbc.la
LOCAL_CFLAGS=-I$(ILBC_DIR)/src
LOCAL_LIBADD=$(ILBCLA)
include $(BASE)/build/modmake.rules

$(ILBCLA): $(ILBC_DIR) $(ILBC_DIR)/.update
	cd $(ILBC_DIR) && $(MAKE)
	$(TOUCH_TARGET)
