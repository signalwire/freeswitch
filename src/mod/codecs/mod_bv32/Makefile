BASE=../../../..

BV32=libbv32-0.1

BV32_DIR=$(BASE)/libs/$(BV32)
LOCAL_CFLAGS=-I$(BV32_DIR)/bv32 -I$(BV32_DIR)/bvcommon
BV32_LA=$(BV32_DIR)/.libs/libbv32.la
LOCAL_LIBADD=$(BV32_LA)
include $(BASE)/build/modmake.rules

$(BV32_DIR):
	$(GETLIB) $(BV32).tar.gz

$(BV32_DIR)/Makefile: $(BV32_DIR)
	cd $(BV32_DIR) && CFLAGS=$(AM_CFLAGS) ./configure --disable-shared --with-pic

$(BV32_LA): $(BV32_DIR)/Makefile
	cd $(BV32_DIR) && $(MAKE)
	$(TOUCH_TARGET)

