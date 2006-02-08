LDFLAGS +=-lilbc

all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install codec/ilbc --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).so $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
