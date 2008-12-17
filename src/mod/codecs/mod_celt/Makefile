BASE=../../../..

CELT=celt-0.5.0

CELT_DIR=$(BASE)/libs/$(CELT)
LOCAL_CFLAGS=-I$(CELT_DIR)/libcelt
CELT_LA=$(CELT_DIR)/libcelt/.libs/libcelt.la
LOCAL_LIBADD=$(CELT_LA)
include $(BASE)/build/modmake.rules

$(CELT_DIR):
	$(GETLIB) $(CELT).tar.gz

$(CELT_DIR)/Makefile: $(CELT_DIR)
	cd $(CELT_DIR) && CFLAGS=$(AM_CFLAGS) ./configure --disable-shared --with-pic

$(CELT_LA): $(CELT_DIR)/Makefile
	cd $(CELT_DIR) && $(MAKE)
	$(TOUCH_TARGET)

