include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_bv

BV_DIR=$(switch_srcdir)/libs/broadvoice
BV_BUILDDIR=$(switch_builddir)/libs/broadvoice
BV_LA=$(BV_BUILDDIR)/src/.libs/libbroadvoice.la
BUILT_SOURCES=$(BV_LA)

mod_LTLIBRARIES = mod_bv.la
mod_bv_la_SOURCES  = mod_bv.c
mod_bv_la_CFLAGS   = $(AM_CFLAGS) -I$(BV_BUILDDIR)/src -I$(BV_DIR)/src 
mod_bv_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(BV_LA)
mod_bv_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(BV_LA): $(BV_DIR)/.update	
	cd $(BV_BUILDDIR) && $(MAKE)
	$(TOUCH_TARGET)
