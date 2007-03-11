CFLAGS+=-I$(BASE)/libs/speex/include
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN)

depends:
	cd $(BASE)/libs/speex && $(MAKE)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c
	$(CC) $(CFLAGS) -c $(MODNAME).c -o $(MODNAME).o
	$(LINK) $(SOLINK) $(MODNAME).o -o $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(LDFLAGS) $(BASE)/libs/speex/libspeex/libspeex.la

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~ .libs

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
