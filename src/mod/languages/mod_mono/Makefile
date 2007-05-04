
MONO = mono-1.2.3.1
MONO_DIR = $(switch_srcdir)/libs/$(MONO)
GLIB = glib-2.12.3
GLIB_DIR = $(switch_srcdir)/libs/$(GLIB)
GETTEXT = gettext-0.15
GETTEXT_DIR = $(switch_srcdir)/libs/$(GETTEXT)
PKGCONFIG = pkg-config-0.21
PKGCONFIG_DIR = $(switch_srcdir)/libs/$(PKGCONFIG)

LOCAL_CFLAGS=-w `$(PREFIX)/bin/pkg-config --cflags mono`
LOCAL_LDFLAGS=`$(PREFIX)/bin/pkg-config --libs mono`
include ../../../../build/modmake.rules

DEFAULT_ARGS=--prefix=$(PREFIX) --with-pic
MONO_ARGS=--with-preview=yes --without-sigaltstack  --disable-mcs-build
MONO_CFLAGS="-I$(GLIB_DIR)/glib -I$(GLIB_DIR)/gmodule -I$(GLIB_DIR)"

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
	cd $(MONO_DIR) && PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig CFLAGS=$(MONO_CFLAGS) top_builddir="$(switch_srcdir)/libs/$(MONO)" ./configure $(DEFAULT_ARGS) $(MONO_ARGS) 
	cd $(MONO_DIR) && $(MAKE) install
