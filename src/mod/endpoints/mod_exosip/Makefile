CFLAGS += -I.
LDFLAGS += -leXosip2  -ljrtp
OBJS=osip_rfc3264.o
ifeq ($(OSARCH),Darwin)
	LINKER=g++
else
	LINKER=$(CC)
endif

all:	depends $(OBJS) $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install jrtplib --prefix=$(PREFIX) --disable-gst
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libosip2-2.2.2.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libeXosip2-2.2.2.tar.gz --disable-josua --prefix=$(PREFIX)


%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(OBJS) $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
