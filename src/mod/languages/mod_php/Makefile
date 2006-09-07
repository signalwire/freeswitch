CFLAGS += -I$(PREFIX)/include/php/TSRM -I$(PREFIX)/include/php/main/ -I$(PREFIX)/include/php/Zend/ -I$(PREFIX)/include/php/
PHPMOD=freeswitch
LDFLAGS += -lphp4

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PHPMOD).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install php-4.4.4.tar.gz --prefix=$(PREFIX) --enable-embed
%.o:  %.c
	$(CC) -fPIC $(CFLAGS) -c $< -o $@

reswig: 
	rm -f switch_swig_wrap.c config.m4 CREDITS *$(PHPMOD)*
	swig -lswitch_swig.i -ignoremissing -DMULTIPLICITY -php -module $(PHPMOD) switch_swig.c
	patch -p0 -i fix.diff

switch_swig_wrap.o: switch_swig_wrap.c
	$(CC) -w $(CFLAGS) -c $< -o $@


$(PHPMOD).$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o
	$(CC) $(SOLINK) -o php_$(PHPMOD).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS)
	$(CC) -fPIC $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
