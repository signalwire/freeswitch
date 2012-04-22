BASE=../../../..

SPEEX_DIR=$(switch_srcdir)/libs/speex
SPEEX_BUILDDIR=$(switch_builddir)/libs/speex
SPEEXLA=$(SPEEX_BUILDDIR)/libspeex/libspeex.la
SPEEXDSPLA=$(SPEEX_BUILDDIR)/libspeex/libspeexdsp.la

LOCAL_CFLAGS=-I$(SPEEX_DIR)/include -I$(SPEEX_BUILDDIR)/include
LOCAL_LIBADD=$(SPEEXLA) $(SPEEXDSPLA)
include $(BASE)/build/modmake.rules

$(SPEEXLA): $(SPEEX_DIR) $(SPEEX_DIR)/.update
	cd $(SPEEX_BUILDDIR)/libspeex && $(MAKE)
	$(TOUCH_TARGET)

$(SPEEXDSPLA): $(SPEEX_DIR) $(SPEEX_DIR)/.update
	cd $(SPEEX_BUILDDIR)/libspeex && $(MAKE)
	$(TOUCH_TARGET)

