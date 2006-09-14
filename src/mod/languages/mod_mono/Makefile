VERSION = mono-1.1.13.8
TARBALL = mono-1.1.13.8.tar.gz
GLIB_TARBALL = glib-2.12.3.tar.gz
GETTEXT_TARBALL = gettext-0.15.tar.gz
PKGCONFIG_TARBALL = pkg-config-0.21.tar.gz
CFLAGS += `$(PREFIX)/bin/pkg-config --cflags mono`
LDFLAGS += `$(PREFIX)/bin/pkg-config --libs mono` 

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install $(GETTEXT_TARBALL) --prefix=$(PREFIX) --with-pic
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install $(PKGCONFIG_TARBALL) --prefix=$(PREFIX) --with-pic
	export PKG_CONFIG=$(PREFIX)/bin/pkg-config; export MSGFMT=$(PREFIX)/bin/; MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install $(GLIB_TARBALL) --prefix=$(PREFIX) --with-pic
	export PATH=$(PATH):$(PREFIX)/bin/; MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install $(TARBALL) --prefix=$(PREFIX) --with-pic

%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
