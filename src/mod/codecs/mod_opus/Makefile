BASE=../../../..

OPUS=opus-1.0.1

OPUS_DIR=$(switch_srcdir)/libs/$(OPUS)
OPUS_BUILDDIR=$(switch_builddir)/libs/$(OPUS)
LOCAL_CFLAGS=-I$(OPUS_DIR)/include -g -O2

OPUS_LA=$(OPUS_BUILDDIR)/.libs/libopus.la

LOCAL_LIBADD=$(OPUS_LA) -lm -lz

include $(BASE)/build/modmake.rules

$(OPUS_DIR):
	$(GETLIB) $(OPUS).tar.gz

$(OPUS_BUILDDIR)/Makefile: $(OPUS_DIR)
	mkdir -p $(OPUS_BUILDDIR)
	cd $(OPUS_BUILDDIR) && $(DEFAULT_VARS) $(OPUS_DIR)/configure --disable-shared --with-pic --srcdir=$(OPUS_DIR)
	$(TOUCH_TARGET)

$(IETF_LA): $(OPUS_BUILDDIR)/Makefile
	cd $(OPUS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(CELT_LA): $(IETF_LA)
$(SILK_LA): $(IETF_LA)

$(MODNAME).lo: $(IETF_LA) $(CELT_LA) $(SILK_LA)
