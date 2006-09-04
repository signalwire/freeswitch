LDFLAGS +=-llpc10

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install codec/lpc10 --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c ../../../include/switch_bitpack.h
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(CC) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
