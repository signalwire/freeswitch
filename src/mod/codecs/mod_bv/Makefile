BASE=../../../..

BV_DIR=$(switch_srcdir)/libs/broadvoice
BV_BUILDDIR=$(switch_builddir)/libs/broadvoice
LOCAL_CFLAGS=-I$(BV_BUILDDIR)/src -I$(BV_DIR)/src 
BV_LA=$(BV_BUILDDIR)/src/.libs/libbroadvoice.la
LOCAL_LIBADD=$(BV_LA)
include $(BASE)/build/modmake.rules

$(BV_LA): $(BV_DIR)/.update	
	cd $(BV_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

