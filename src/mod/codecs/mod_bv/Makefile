BASE=../../../..
BV_DIR=$(BASE)/libs/broadvoice
BV_SRCDIR=$(MODDIR)/../../../../libs/broadvoice
LOCAL_CFLAGS=-I$(BV_SRCDIR)/src -I$(BV_DIR)/src 
BV_LA=$(BV_DIR)/src/.libs/libbroadvoice.la
LOCAL_LIBADD=$(BV_LA)
include $(BASE)/build/modmake.rules

$(BV_LA): $(BV_SRCDIR)/.update	
	cd $(BV_DIR) && $(MAKE)
	$(TOUCH_TARGET)

