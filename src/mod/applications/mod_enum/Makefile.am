include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_enum

LDNS=ldns
LDNS_DIR=$(switch_srcdir)/libs/$(LDNS)
LDNS_BUILDDIR=$(switch_builddir)/libs/$(LDNS)
LDNS_LA=$(LDNS_BUILDDIR)/libldns.la

mod_LTLIBRARIES = mod_enum.la
mod_enum_la_SOURCES  = mod_enum.c
mod_enum_la_CFLAGS   = $(AM_CFLAGS) -I$(LDNS_DIR) -I$(LDNS_DIR)/include
mod_enum_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(LDNS_LA)
mod_enum_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(LDNS_BUILDDIR)/ldns/util.h

$(LDNS_BUILDDIR)/ldns/util.h $(LDNS_BUILDDIR)/Makefile: $(LDNS_DIR)/Makefile.in
	mkdir -p $(LDNS_BUILDDIR)
	cd $(LDNS_BUILDDIR) && $(DEFAULT_VARS) $(LDNS_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(LDNS_DIR) --disable-gost --without-ssl --disable-sha2 --disable-shared --with-pic
	$(TOUCH_TARGET)

$(LDNS_LA): $(LDNS_BUILDDIR)/Makefile
	cd $(LDNS_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
