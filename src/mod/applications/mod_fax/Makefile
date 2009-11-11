BASE=../../../..

TIFF_DIR=$(BASE)/libs/tiff-3.8.2
TIFF_LA=$(TIFF_DIR)/libtiff/libtiff.la

SPANDSP_DIR=$(BASE)/libs/spandsp
SPANDSP_LA=$(SPANDSP_DIR)/src/libspandsp.la

LOCAL_CFLAGS=-I$(SPANDSP_DIR)/src -I$(TIFF_DIR)/libtiff
LOCAL_LIBADD=$(SPANDSP_LA)

include $(BASE)/build/modmake.rules

$(SPANDSP_LA): $(TIFF_LA) $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_DIR) && $(MAKE)
	$(TOUCH_TARGET)

$(TIFF_LA): $(TIFF_DIR) $(TIFF_DIR)/.update
	cd $(TIFF_DIR) && $(MAKE)
	$(TOUCH_TARGET)


