CFLAGS += -I$(BASE)/libs/curl/include
LDFLAGS += $(BASE)/libs/curl/lib/libcurl.la
include sm.mak

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/curl && $(MAKE)
	cd $(BASE)/libs/js/nsprpub && $(MAKE)
	cd $(BASE)/libs/js && $(MAKE)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).h
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	cd $(BASE)/libs/js/nsprpub && $(MAKE) install
	cd $(BASE)/libs/js && $(MAKE) install
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
