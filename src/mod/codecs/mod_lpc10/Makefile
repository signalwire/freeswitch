CFLAGS+=-I$(BASE)/libs/codec/lpc10/src

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/codec/lpc10 && $(MAKE)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c ../../../include/switch_bitpack.h
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS) $(BASE)/libs/codec/lpc10/liblpc10.la

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
