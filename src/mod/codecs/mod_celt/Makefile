BASE=../../../..

CELT=celt-0.7.1

CELT_DIR=$(switch_srcdir)/libs/$(CELT)
CELT_BUILDDIR=$(switch_builddir)/libs/$(CELT)
LOCAL_CFLAGS=-I$(CELT_DIR)/libcelt
CELT_LA=$(CELT_BUILDDIR)/libcelt/.libs/libcelt0.la
LOCAL_LIBADD=$(CELT_LA)
include $(BASE)/build/modmake.rules

$(CELT_DIR):
	$(GETLIB) $(CELT).tar.gz

$(CELT_BUILDDIR)/Makefile: $(CELT_DIR)
	mkdir -p $(CELT_BUILDDIR)
	cd $(CELT_BUILDDIR) && $(DEFAULT_VARS) $(CELT_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(CELT_DIR)

$(CELT_LA): $(CELT_BUILDDIR)/Makefile
	cd $(CELT_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

