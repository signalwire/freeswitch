include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_silk

SILK_DIR=$(switch_srcdir)/libs/silk
SILK_BUILDDIR=$(switch_builddir)/libs/silk
SILK_LA=$(SILK_BUILDDIR)/.libs/libSKP_SILK_SDK.la

mod_LTLIBRARIES = mod_silk.la
mod_silk_la_SOURCES  = mod_silk.c
mod_silk_la_CFLAGS   = $(AM_CFLAGS) -I$(SILK_DIR)/src  -I$(SILK_DIR)/interface
mod_silk_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SILK_LA)
mod_silk_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(SILK_LA)

$(SILK_LA): $(SILK_DIR)/.update	
	mkdir -p $(SILK_BUILDDIR)
	cd $(SILK_BUILDDIR) && $(DEFAULT_VARS) $(SILK_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(SILK_DIR) --disable-shared && $(MAKE) clean && $(MAKE)
	$(TOUCH_TARGET)
