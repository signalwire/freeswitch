include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skypopen

SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_skypopen.la
mod_skypopen_la_SOURCES  = mod_skypopen.c skypopen_protocol.c
mod_skypopen_la_CFLAGS   = $(AM_CFLAGS)
mod_skypopen_la_CPPFLAGS = -I$(SPANDSP_DIR)/src -I$(SPANDSP_BUILDDIR)/src -I.
mod_skypopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA)
mod_skypopen_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lX11

BUILT_SOURCES = $(SPANDSP_LA)

$(SPANDSP_LA): $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_BUILDDIR) && $(MAKE) CFLAGS="$(CFLAGS)" CPPFLAGS="$(CPPFLAGS)"
	$(TOUCH_TARGET)
