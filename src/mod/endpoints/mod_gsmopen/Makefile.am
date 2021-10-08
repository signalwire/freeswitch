include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_gsmopen

SPANDSP_DIR=$(switch_srcdir)/libs/spandsp
SPANDSP_BUILDDIR=$(switch_builddir)/libs/spandsp
SPANDSP_LA=$(SPANDSP_BUILDDIR)/src/libspandsp.la

mod_LTLIBRARIES = mod_gsmopen.la
mod_gsmopen_la_SOURCES  = mod_gsmopen.cpp gsmopen_protocol.cpp 
mod_gsmopen_la_CXXFLAGS = $(SWITCH_AM_CXXFLAGS)
mod_gsmopen_la_CPPFLAGS = -I$(SPANDSP_DIR)/src -I$(SPANDSP_BUILDDIR)/src -I.
mod_gsmopen_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LA)
mod_gsmopen_la_LDFLAGS  = -avoid-version -module -no-undefined -lctb-0.16 -lgsmme

BUILT_SOURCES = $(SPANDSP_LA)

$(SPANDSP_LA): $(SPANDSP_DIR) $(SPANDSP_DIR)/.update
	cd $(SPANDSP_BUILDDIR) && $(MAKE) CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)"
	$(TOUCH_TARGET)
