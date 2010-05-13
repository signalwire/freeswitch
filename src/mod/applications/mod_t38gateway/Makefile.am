include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_t38gateway

TIFF_DIR=$(switch_srcdir)/libs/tiff-3.8.2
TIFF_BUILDDIR=$(switch_builddir)/libs/tiff-3.8.2
TIFF_LA=$(TIFF_BUILDDIR)/libtiff/libtiff.la

SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_t38gateway.la
mod_t38gateway_la_SOURCES  = mod_t38gateway.c udptl.c
mod_t38gateway_la_CFLAGS   = $(AM_CFLAGS) -I$(SPANDSP_DIR)/src -I$(TIFF_DIR)/libtiff -I$(SPANDSP_BUILDDIR)/src -I$(TIFF_BUILDDIR)/libtiff
mod_t38gateway_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA) $(TIFF_LA)
mod_t38gateway_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -ljpeg

