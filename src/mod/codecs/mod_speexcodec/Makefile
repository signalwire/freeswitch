all:	depends $(MODNAME).so

depends:
	$(BASE)/buildlib.sh $(BASE) install speex-1.1.11.1.tar.gz --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).so $(LDFLAGS) -lspeex

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
