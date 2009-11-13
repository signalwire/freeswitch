BASE=../../../..

BV16=libbv16-0.1

BV16_DIR=$(BASE)/libs/$(BV16)
LOCAL_CFLAGS=-I$(BV16_DIR)/bv16 -I$(BV16_DIR)/bvcommon
BV16_LA=$(BV16_DIR)/.libs/libbv16.la
LOCAL_LIBADD=$(BV16_LA)
include $(BASE)/build/modmake.rules

$(BV16_DIR):
	$(GETLIB) $(BV16).tar.gz

$(BV16_DIR)/Makefile: $(BV16_DIR)
	cd $(BV16_DIR) && CFLAGS=$(AM_CFLAGS) ./configure --disable-shared --with-pic

$(BV16_LA): $(BV16_DIR)/Makefile
	cd $(BV16_DIR) && $(MAKE)
	$(TOUCH_TARGET)

