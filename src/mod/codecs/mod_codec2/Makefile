BASE=../../../..

CODEC2_DIR=$(switch_srcdir)/libs/libcodec2-1.0
CODEC2_BUILDDIR=$(switch_builddir)/libs/libcodec2-1.0
CODEC2LA=$(CODEC2_BUILDDIR)/src/libcodec2.la

LOCAL_CFLAGS=-I$(CODEC2_DIR)/include -I$(CODEC2_BUILDDIR)/src
LOCAL_LIBADD=$(CODEC2LA)
include $(BASE)/build/modmake.rules

$(CODEC2LA): $(CODEC2_DIR) $(CODEC2_DIR)/.update
	cd $(CODEC2_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

