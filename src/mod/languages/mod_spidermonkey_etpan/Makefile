include ../mod_spidermonkey/sm.mak

OBJS=#$(BASE)/libs/mozilla/js/src/$(OS_CONFIG)_$(VER).OBJ/libjs.a $(BASE)/libs/mozilla/nsprpub/dist/lib/libnspr4.a  -letpan
LINKER=$(CC)


all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libetpan-0.46.tar.gz --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
