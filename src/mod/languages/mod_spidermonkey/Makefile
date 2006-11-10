include sm.mak
OBJS=$(BASE)/libs/mozilla/js/src/$(OS_CONFIG)_$(VER).OBJ/libjs.a $(BASE)/libs/mozilla/nsprpub/dist/lib/libnspr4.a -lcurl -letpan
LINKER=$(CC)


all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	BUILD_OPT=$(BO) MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) mozilla.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install curl-7.15.2.tar.gz --prefix=$(PREFIX) --without-libidn
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libetpan-0.45.tar.gz --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
