#CFLAGS += -I/usr/src/common/src
LDFLAGS += -leXosip2  -ljrtp

ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install jrtplib --prefix=$(PREFIX) --disable-gst
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libosip2-2.2.2.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libeXosip2-2.2.2.tar.gz --disable-josua --prefix=$(PREFIX)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
