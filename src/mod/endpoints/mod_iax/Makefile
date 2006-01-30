LDFLAGS += -liax -L/usr/local/lib

all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/buildlib.sh $(BASE) install iax --enable-newjb --prefix=$(PREFIX)

$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)


clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
