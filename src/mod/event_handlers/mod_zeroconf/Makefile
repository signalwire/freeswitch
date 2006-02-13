LDFLAGS += -lhowl
CFLAGS += -I$(PREFIX)/include/howl
LINKER=$(CC)


all:	depends $(MODNAME).so

depends:
	$(BASE)/build/buildlib.sh $(BASE) install howl-1.0.0.tar.gz --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
