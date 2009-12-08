BASE=../../../..

CELT=celt-0.7.0

CELT_DIR=$(switch_srcdir)/libs/$(CELT)
CELT_BUILDDIR=$(switch_builddir)/libs/$(CELT)
LOCAL_CFLAGS=-I$(CELT_DIR)/libcelt
CELT_LA=$(CELT_BUILDDIR)/libcelt/.libs/libcelt.la
LOCAL_LIBADD=$(CELT_LA)
include $(BASE)/build/modmake.rules

$(CELT_DIR):
	$(GETLIB) $(CELT).tar.gz

$(CELT_DIR)/Makefile: $(CELT_DIR)
	cd $(CELT_DIR) && $(DEFAULT_VARS) ./configure $(DEFAULT_ARGS)

$(CELT_LA): $(CELT_DIR)/Makefile
	cd $(CELT_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

