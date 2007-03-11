LDFLAGS += -lhowl
CFLAGS += -I$(PREFIX)/include/howl
LINKER=$(CC)


all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	$(BASE)/build/buildlib.sh $(BASE) install howl-1.0.0.tar.gz --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o 
	$(LINK) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
