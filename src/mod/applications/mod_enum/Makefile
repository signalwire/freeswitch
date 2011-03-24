# LDNS=ldns-1.6.9
# BASE=../../../..

# LDNS_DIR=$(switch_srcdir)/libs/$(LDNS)
# LDNS_BUILDDIR=$(switch_builddir)/libs/$(LDNS)
# LDNSLA=$(LDNS_BUILDDIR)/libldns.la

# include $(BASE)/build/modmake.rules
# LOCAL_LIBADD=$(LDNSLA)


BASE=../../../..

LDNS=ldns-1.6.9

LDNS_DIR=$(switch_srcdir)/libs/$(LDNS)
LDNS_BUILDDIR=$(switch_builddir)/libs/$(LDNS)
LOCAL_CFLAGS=-I$(LDNS_DIR) -I$(LDNS_DIR)/include
LDNS_LA=$(LDNS_BUILDDIR)/libldns.la
LOCAL_LIBADD=$(LDNS_LA)
include $(BASE)/build/modmake.rules

$(LDNS_DIR)/Makefile.in:
	$(GETLIB) $(LDNS).tar.gz

$(LDNS_BUILDDIR)/Makefile: $(LDNS_DIR)/Makefile.in
	mkdir -p $(LDNS_BUILDDIR)
	cd $(LDNS_BUILDDIR) && $(DEFAULT_VARS) $(LDNS_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(LDNS_DIR) --disable-gost
	$(TOUCH_TARGET)

$(LDNS_LA): $(LDNS_BUILDDIR)/Makefile
	cd $(LDNS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

