include ../mod_spidermonkey/sm.mak
LDFLAGS+=-lodbc

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install unixODBC-2.2.12.tar.gz --prefix=$(PREFIX) --disable-gui --without-x --with-pic

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -fPIC -c $(MODNAME).c -o $(MODNAME).o 
	$(CC) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
