BASE=../../../..

SPANDSP_DIR=$(BASE)/libs/spandsp
SPANDSP_LA=$(SPANDSP_DIR)/src/libspandsp.la

LOCAL_CFLAGS=-I$(SPANDSP_DIR)/src
LOCAL_LIBADD=$(SPANDSP_LA)

include $(BASE)/build/modmake.rules

$(SPANDSP_LA): $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_DIR) && $(MAKE)
	$(TOUCH_TARGET)


depend_install: $(DESTDIR)$(libdir)/libspandsp.la

$(DESTDIR)$(libdir)/libspandsp.la: $(SPANDSP_LA)
	cd $(SPANDSP_DIR) && $(MAKE) install


