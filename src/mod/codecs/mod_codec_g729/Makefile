LDFLAGS +=-lg729

all:	depends $(MODNAME).so

depends:
	$(BASE)/buildlib.sh $(BASE) install codec/libg729 --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).so $(LDFLAGS) -lspeex

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
