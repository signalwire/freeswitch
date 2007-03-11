LDFLAGS += $(BASE)/libs/libdingaling/libdingaling.la
CFLAGS  += -I$(BASE)/libs/libdingaling/src
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/iksemel && $(MAKE)
	cd $(BASE)/libs/libdingaling && $(MAKE)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	cd $(BASE)/libs/iksemel && $(MAKE) install
	cd $(BASE)/libs/libdingaling && $(MAKE) install
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
