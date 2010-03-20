BASE=../../../..
SILK_DIR=$(switch_srcdir)/libs/silk
SILK_BUILDDIR=$(switch_builddir)/libs/silk
LOCAL_CFLAGS=-I$(SILK_DIR)/src  -I$(SILK_DIR)/interface
SILK_LA=$(SILK_BUILDDIR)/.libs/libSKP_SILK_SDK.la
LOCAL_LIBADD=$(SILK_LA)
include $(BASE)/build/modmake.rules

$(SILK_LA): $(SILK_DIR)/.update	
	cd $(SILK_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
