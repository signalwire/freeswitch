
MONO = mono-1.1.13.8
MONO_DIR = $(switch_srcdir)/libs/$(MONO)
GLIB = glib-2.12.3
GLIB_DIR = $(switch_srcdir)/libs/$(GLIB)
GETTEXT = gettext-0.15
GETTEXT_DIR = $(switch_srcdir)/libs/$(GETTEXT)
PKGCONFIG = pkg-config-0.21
PKGCONFIG_DIR = $(switch_srcdir)/libs/$(PKGCONFIG)

LOCAL_CFLAGS=`$(PREFIX)/bin/pkg-config --cflags mono`
LOCAL_LDFLAGS=`$(PREFIX)/bin/pkg-config --libs mono`
include ../../../../build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --with-pic

local_depend: $(GETTEXT_DIR) $(PKGCONFIG_DIR) $(GLIB_DIR) $(MONO_DIR)

$(GETTEXT_DIR):
	$(GETLIB) $(GETTEXT).tar.gz
	cd $(GETTEXT_DIR) && ./configure $(DEFAULT_ARGS) 
	cd $(GETTEXT_DIR) && $(MAKE) install

$(PKGCONFIG_DIR):
	$(GETLIB) $(PKGCONFIG).tar.gz
	cd $(PKGCONFIG_DIR) && ./configure $(DEFAULT_ARGS) 
	cd $(PKGCONFIG_DIR) && $(MAKE) install

$(GLIB_DIR):
	$(GETLIB) $(GLIB).tar.gz
	export PKG_CONFIG=$(PREFIX)/bin/pkg-config
	export MSGFMT=$(PREFIX)/bin/
	cd $(GLIB_DIR) && ./configure $(DEFAULT_ARGS) 
	cd $(GLIB_DIR) && $(MAKE) install

$(MONO_DIR):
	$(GETLIB) $(MONO).tar.gz
	export PATH=$(PATH):$(PREFIX)/bin/
	cd $(MONO_DIR) && ./configure $(DEFAULT_ARGS) --with-pic --with-gc=boehm --with-preview=yes --without-sigaltstack 
	cd $(MONO_DIR) && $(MAKE) install
