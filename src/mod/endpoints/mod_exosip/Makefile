#CFLAGS += -I/usr/src/common/src
LDFLAGS += -leXosip2  -ljrtp

ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

all:	depends $(MODNAME).so

depends:
	MAKE=$(MAKE) $(BASE)/buildlib.sh $(BASE) install jrtplib --prefix=$(PREFIX) --disable-gst
	MAKE=$(MAKE) $(BASE)/buildlib.sh $(BASE) install libosip2-2.2.2.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/buildlib.sh $(BASE) install libeXosip2-2.2.2.tar.gz --disable-josua --prefix=$(PREFIX)


$(MODNAME).so: $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).so  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.so *.o *~

install:
	cp -f $(MODNAME).so $(PREFIX)/mod
