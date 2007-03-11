VERSION = mono-1.1.13.8
TARBALL = mono-1.1.13.8.tar.gz
GLIB_TARBALL = glib-2.12.3.tar.gz
GETTEXT_TARBALL = gettext-0.15.tar.gz
PKGCONFIG_TARBALL = pkg-config-0.21.tar.gz
CFLAGS += `$(PREFIX)/bin/pkg-config --cflags mono`
LDFLAGS += `$(PREFIX)/bin/pkg-config --libs mono` 

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) configure install $(GETTEXT_TARBALL) --prefix=$(PREFIX) --with-pic
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) configure install $(PKGCONFIG_TARBALL) --prefix=$(PREFIX) --with-pic
	export PKG_CONFIG=$(PREFIX)/bin/pkg-config; export MSGFMT=$(PREFIX)/bin/; MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) configure install $(GLIB_TARBALL) --prefix=$(PREFIX) --with-pic
	export PATH=$(PATH):$(PREFIX)/bin/; MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) configure install $(TARBALL) --prefix=$(PREFIX) --with-pic --with-gc=boehm --with-preview=yes --without-sigaltstack

%.o:  %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
