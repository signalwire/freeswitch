include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_spandsp

TIFF_DIR=$(switch_srcdir)/libs/tiff-4.0.2
TIFF_BUILDDIR=$(switch_builddir)/libs/tiff-4.0.2
TIFF_LA=$(TIFF_BUILDDIR)/libtiff/libtiff.la
BUILD_CFLAGS=
SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_spandsp.la
mod_spandsp_la_SOURCES  = mod_spandsp.c udptl.c mod_spandsp_fax.c mod_spandsp_dsp.c mod_spandsp_codecs.c mod_spandsp_modem.c
mod_spandsp_la_CFLAGS   = $(BUILD_CFLAGS) $(AM_CFLAGS) -I$(SPANDSP_DIR)/src -I$(TIFF_DIR)/libtiff -I$(SPANDSP_BUILDDIR)/src -I$(TIFF_BUILDDIR)/libtiff -I.
mod_spandsp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA) $(TIFF_LA) $(SPANDSP_LA_JBIG) $(SPANDSP_LA_LZMA) -ljpeg -lz -lutil
mod_spandsp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(SPANDSP_LA): $(TIFF_LA) $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_BUILDDIR) && $(MAKE) -j1
	$(TOUCH_TARGET)

$(TIFF_LA): $(TIFF_DIR) $(TIFF_DIR)/.update
	cd $(TIFF_BUILDDIR) && $(MAKE) -j1
	$(TOUCH_TARGET)
