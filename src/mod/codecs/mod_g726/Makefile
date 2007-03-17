BASE=../../../..
G726_DIR=$(BASE)/libs/codec/g726
G726LA=$(G726_DIR)/libg726.la
LOCAL_CFLAGS=-I$(G726_DIR)/src
LOCAL_LIBADD=$(G726LA)
include $(BASE)/build/modmake.rules

$(G726LA): $(G726_DIR) $(G726_DIR)/.update
	cd $(G726_DIR) && $(MAKE)
	$(TOUCH_TARGET)
