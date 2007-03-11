CFLAGS += -I$(BASE)/libs/codec/g726/src

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/codec/g726 && $(MAKE)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c ../../../include/switch_bitpack.h
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS) $(BASE)/libs/codec/g726/libg726.la

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
