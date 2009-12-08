BASE=../../../..
ILBC_DIR=$(switch_srcdir)/libs/ilbc
ILBC_BUILDDIR=$(switch_builddir)/libs/ilbc
ILBCLA=$(ILBC_BUILDDIR)/src/libilbc.la
LOCAL_CFLAGS=-I$(ILBC_DIR)/src -I$(ILBC_BUILDDIR)/src
LOCAL_LIBADD=$(ILBCLA)

include $(BASE)/build/modmake.rules

$(ILBCLA): $(ILBC_DIR) $(ILBC_DIR)/.update
	cd $(ILBC_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
