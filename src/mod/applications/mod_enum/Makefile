BASE=../../../..
UDNS_DIR=$(BASE)/libs/udns
UDNSA=$(UDNS_DIR)/libudns.a
LOCAL_CFLAGS=-I$(UDNS_DIR)
LOCAL_LIBADD=$(UDNSA)
include $(BASE)/build/modmake.rules

$(UDNSA): $(UDNS_DIR) $(UDNS_DIR)/.update
	cd $(UDNS_DIR) && $(MAKE) staticlib
	$(TOUCH_TARGET)

