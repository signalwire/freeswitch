#Usage: /usr/local/freeswitch/bin/php-config [--prefix|--includes|--ldflags|--libs|--extension-dir|--include-dir|--php-binary|--version]
PCFG=$(PREFIX)/bin/php-config

LCFLAGS=-fPIC -DZTS -DPTHREADS
CFLAGS += `$(PCFG) --includes`
MDIR += `$(PCFG) --extension-dir`
PHPMOD=freeswitch
PHPLDFLAGS = `$(PCFG) --ldflags` -lcrypt -lresolv -lm -ldl -lnsl -lxml2 -lz -lphp5

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PHPMOD).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install php-5.1.6.tar.gz --prefix=$(PREFIX) --enable-embed=static --enable-static --with-pic --with-mysql --with-curl
%.o:  %.c
	$(CC) $(LCFLAGS) $(CFLAGS) -c $< -o $@

reswig: 
	rm -f switch_swig_wrap.c config.m4 CREDITS *$(PHPMOD)*
	swig -lswitch_swig.i -ignoremissing -DMULTIPLICITY -php -module $(PHPMOD) switch_swig.c
	patch -p0 -i fix.diff

switch_swig_wrap.o: switch_swig_wrap.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

switch_swig.o: switch_swig.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@


$(PHPMOD).$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o Makefile
	$(CC) $(SOLINK) -o php_$(PHPMOD).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS) Makefile
	$(CC) $(LCFLAGS) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS) $(PHPLDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	mkdir -p $(MDIR)
	cp -f php_$(PHPMOD).$(DYNAMIC_LIB_EXTEN) $(MDIR)
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
	cp -f $(PHPMOD).php $(PREFIX)/lib/php
