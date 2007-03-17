BASE=../../../..

IKSEMEL_DIR=$(BASE)/libs/iksemel
DING_DIR=$(BASE)/libs/libdingaling
IKSEMELLA=$(IKSEMEL_DIR)/src/libiksemel.la
DINGLA=$(DING_DIR)/libdingaling.la

LOCAL_CFLAGS=-I$(DING_DIR)/src
LOCAL_LIBADD=$(DINGLA)

include $(BASE)/build/modmake.rules

INST_DINGLA=$(PREFIX)/lib/libdingaling.la

$(IKSEMELLA): $(IKSEMEL_DIR) $(IKSEMEL_DIR)/.update
	cd $(IKSEMEL_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(DINGLA): $(IKSEMELLA) $(DING_DIR) $(DING_DIR)/.update
	cd $(DING_DIR) && $(MAKE)
	$(TOUCH_TARGET)

depend_install: $(INST_DINGLA)

$(INST_DINGLA): $(DINGLA)
	cd $(DING_DIR) && $(MAKE) install

