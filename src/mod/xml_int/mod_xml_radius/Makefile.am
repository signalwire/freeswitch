include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_xml_radius

RADCLIENT_VERSION=1.1.6
RADCLIENT=freeradius-client-$(RADCLIENT_VERSION)
RADCLIENT_DIR=$(switch_srcdir)/libs/$(RADCLIENT)
RADCLIENT_BUILDDIR=$(switch_builddir)/libs/$(RADCLIENT)
RADCLIENT_LIBDIR=$(RADCLIENT_BUILDDIR)/lib
RADCLIENT_LA=${RADCLIENT_LIBDIR}/libfreeradius-client.la

mod_LTLIBRARIES = mod_xml_radius.la
mod_xml_radius_la_SOURCES  = mod_xml_radius.c
mod_xml_radius_la_CFLAGS   = $(AM_CFLAGS) -I$(RADCLIENT_DIR)/include
mod_xml_radius_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(RADCLIENT_LA)
mod_xml_radius_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

BUILT_SOURCES=$(RADCLIENT_LA)

$(RADCLIENT_DIR):
	$(GETLIB) $(RADCLIENT).tar.gz

$(RADCLIENT_BUILDDIR)/Makefile: $(RADCLIENT_DIR)
	mkdir -p $(RADCLIENT_BUILDDIR)
	cd $(RADCLIENT_BUILDDIR) && $(DEFAULT_VARS) $(RADCLIENT_DIR)/configure $(DEFAULT_ARGS) --srcdir=$(RADCLIENT_DIR)
	$(TOUCH_TARGET)

$(RADCLIENT_LA): $(RADCLIENT_BUILDDIR)/Makefile
	cd $(RADCLIENT_BUILDDIR) && CFLAGS="$(CFLAGS)" $(MAKE)
	$(TOUCH_TARGET)


