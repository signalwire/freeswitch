include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skypopen

TIFF_DIR=$(switch_srcdir)/libs/tiff-4.0.2
TIFF_BUILDDIR=$(switch_builddir)/libs/tiff-4.0.2
TIFF_LA=$(TIFF_BUILDDIR)/libtiff/libtiff.la

SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_skypopen.la
mod_skypopen_la_SOURCES  = mod_skypopen.c skypopen_protocol.c
mod_skypopen_la_CFLAGS   = $(AM_CFLAGS) -I$(SPANDSP_DIR)/src -I$(TIFF_DIR)/libtiff -I$(SPANDSP_BUILDDIR)/src -I$(TIFF_BUILDDIR)/libtiff -I. -DSKYPOPEN_C_VER=\"`git log -1 --format="%h" skypopen_protocol.c`\" -DMODSKYPOPEN_C_VER=\"`git log -1 --format="%h" mod_skypopen.c`\"
mod_skypopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA) $(TIFF_LA)
mod_skypopen_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lX11

$(SPANDSP_LA): $(TIFF_LA) $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_BUILDDIR) && $(MAKE) -j1
	$(TOUCH_TARGET)

$(TIFF_LA): $(TIFF_DIR) $(TIFF_DIR)/.update
	cd $(TIFF_BUILDDIR) && $(MAKE) -j1
	$(TOUCH_TARGET)
