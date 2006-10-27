OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))
VERSION = sofia-sip-1.12
TARBALL = sofia-sip-1.12.3.6.tar.gz
CFLAGS += -I. -I$(PREFIX)/include/$(VERSION)
LDFLAGS += -lsofia-sip-ua
LINKER=$(CC)
ifeq ($(OS_ARCH),Darwin)
	LDFLAGS +=-framework CoreFoundation -framework SystemConfiguration
endif
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install $(TARBALL) --prefix=$(PREFIX) --with-pic --with-glib=no

%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
