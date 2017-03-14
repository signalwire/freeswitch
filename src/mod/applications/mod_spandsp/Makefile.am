include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_spandsp

SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_spandsp.la
mod_spandsp_la_SOURCES  = mod_spandsp.c udptl.c mod_spandsp_fax.c mod_spandsp_dsp.c mod_spandsp_codecs.c mod_spandsp_modem.c
mod_spandsp_la_CFLAGS   = $(AM_CFLAGS) 
mod_spandsp_la_CPPFLAGS = -I$(SPANDSP_DIR)/src -I$(SPANDSP_BUILDDIR)/src -I.
mod_spandsp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA) $(SPANDSP_LA_JBIG) $(SPANDSP_LA_LZMA) -ljpeg -lz -ltiff
mod_spandsp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(SPANDSP_LA): $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_BUILDDIR) && $(MAKE) CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)"
	$(TOUCH_TARGET)
