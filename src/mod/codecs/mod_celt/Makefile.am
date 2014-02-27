include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_celt

CELT=celt-0.10.0

CELT_DIR=$(switch_srcdir)/libs/$(CELT)
CELT_BUILDDIR=$(switch_builddir)/libs/$(CELT)
CELT_LA=$(CELT_BUILDDIR)/libcelt/.libs/libcelt0.la

mod_LTLIBRARIES      = mod_celt.la
mod_celt_la_SOURCES  = mod_celt.c
mod_celt_la_CFLAGS   = $(AM_CFLAGS) -I$(CELT_DIR)/libcelt
mod_celt_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(CELT_LA)
mod_celt_la_LDFLAGS = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(CELT_LA)
$(CELT_DIR):
	$(GETLIB) $(CELT).tar.gz

$(CELT_BUILDDIR)/Makefile: $(CELT_DIR)
	mkdir -p $(CELT_BUILDDIR)
	cd $(CELT_BUILDDIR) && $(DEFAULT_VARS) $(CELT_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(CELT_DIR)
	$(TOUCH_TARGET)

$(CELT_LA): $(CELT_BUILDDIR)/Makefile
	cd $(CELT_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)

$(MODNAME).lo: $(CELT_LA)
