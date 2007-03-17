BASE=../../../..
G7XX_DIR=$(BASE)/libs/codec/g7xx
G7XXLA=$(G7XX_DIR)/libg7xx.la
LOCAL_CFLAGS=-I$(G7XX_DIR)/src/include/
LOCAL_LIBADD=$(G7XXLA)
include $(BASE)/build/modmake.rules

$(G7XXLA): $(G7XX_DIR) $(G7XX_DIR)/.update
	cd $(G7XX_DIR) && $(MAKE)
	$(TOUCH_TARGET)
