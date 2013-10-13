BASE=../../../..

OPUS=opus-1.0.3

OPUS_DIR=$(switch_srcdir)/libs/$(OPUS)
OPUS_BUILDDIR=$(switch_builddir)/libs/$(OPUS)
LOCAL_CFLAGS=-I$(OPUS_DIR)/include -g -O2
LOCAL_LDFLAGS=-lm -lz

OPUS_LA=$(OPUS_BUILDDIR)/.libs/libopus.la

LOCAL_LIBADD=$(OPUS_LA)

include $(BASE)/build/modmake.rules

$(OPUS_DIR):
	$(GETLIB) $(OPUS).tar.gz

$(OPUS_DIR)/Makefile: $(OPUS_DIR)
	cd $(OPUS_BUILDDIR) && $(DEFAULT_VARS) $(OPUS_DIR)/configure --disable-shared --with-pic --srcdir=$(OPUS_DIR)
	$(TOUCH_TARGET)

$(OPUS_LA): $(OPUS_DIR)/Makefile
	cd $(OPUS_BUILDDIR) && $(MAKE)



$(MODNAME).lo: $(OPUS_LA)

