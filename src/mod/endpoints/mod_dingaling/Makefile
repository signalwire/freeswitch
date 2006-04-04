LDFLAGS += -ldingaling

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install iksemel-1.2.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install libdingaling --prefix=$(PREFIX)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)


clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ *.so

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
