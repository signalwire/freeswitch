CFLAGS += -DWITH_OPENLDAP -DLDAP_DEPRECATED
LDFLAGS += -lldap_r -llber

LINKER=$(CC)


all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install openldap-2.3.19.tgz --prefix=$(PREFIX) --disable-bdb --disable-hdb

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(MODNAME).o $(LDFLAGS) 

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
