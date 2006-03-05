LDFLAGS += -lhowl
CFLAGS += -I$(PREFIX)/include/howl
LINKER=$(CC)


all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	$(BASE)/build/buildlib.sh $(BASE) install howl-1.0.0.tar.gz --prefix=$(PREFIX)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(LINKER) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
