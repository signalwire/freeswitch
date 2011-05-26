BASE=../../../..

LDNS=ldns

LDNS_DIR=$(switch_srcdir)/libs/$(LDNS)
LDNS_BUILDDIR=$(switch_builddir)/libs/$(LDNS)
LOCAL_CFLAGS=-I$(LDNS_DIR) -I$(LDNS_DIR)/include
LDNS_LA=$(LDNS_BUILDDIR)/libldns.la
LOCAL_LIBADD=$(LDNS_LA)
include $(BASE)/build/modmake.rules

$(LDNS_BUILDDIR)/Makefile: $(LDNS_DIR)/Makefile.in
	mkdir -p $(LDNS_BUILDDIR)
	cd $(LDNS_BUILDDIR) && $(DEFAULT_VARS) $(LDNS_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(LDNS_DIR) --disable-gost --without-ssl --disable-sha2
	$(TOUCH_TARGET)

$(LDNS_LA): $(LDNS_BUILDDIR)/Makefile
	cd $(LDNS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(MODNAME).lo: $(LDNS_LA)
