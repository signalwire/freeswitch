CFLAGS += -DWITH_OPENLDAP -DLDAP_DEPRECATED
LDFLAGS += -lldap_r -llber

LINKER=$(CC)


all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install openldap-2.3.19.tgz --prefix=$(PREFIX) --disable-bdb --disable-hdb

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
