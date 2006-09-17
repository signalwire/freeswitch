#Usage: /usr/local/freeswitch/bin/php-config [--prefix|--includes|--ldflags|--libs|--extension-dir|--include-dir|--php-binary|--version]
PCFG=$(PREFIX)/bin/php-config

LCFLAGS=-fPIC -DZTS -DPTHREADS
# we should NOT use -fno-strict-aliasing.  we need to fix this issue, but it is an issue in the php header files.  we need to be careful of the optimization level on this module becuase of this setting.
CFLAGS += `$(PCFG) --includes` -g3 -fno-strict-aliasing
MDIR += `$(PCFG) --extension-dir`
PHPMOD=freeswitch
PHPLDFLAGS = `$(PCFG) --ldflags` -lm -ldl -lxml2 -lz -lphp5
MOD_CFLAGS += -fPIC
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PHPMOD).$(DYNAMIC_LIB_EXTEN)

depends:
	MOD_CFLAGS="$(MOD_CFLAGS)" MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install zlib-1.2.3.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install curl-7.15.2.tar.gz --prefix=$(PREFIX)
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install php-5.1.6.tar.gz --prefix=$(PREFIX) --enable-embed=shared --enable-shared --with-pic --with-zlib=$(PREFIX) --with-curl --enable-maintainer-zts --with-tsrm-pthreads --enable-debug
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
	#mkdir -p $(MDIR)
	#cp -f php_$(PHPMOD).$(DYNAMIC_LIB_EXTEN) $(MDIR)
	cp -f php_$(PHPMOD).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/lib/php/extensions
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
	cp -f $(PHPMOD).php $(PREFIX)/lib/php
	cp -f classFreeswitch.php $(PREFIX)/lib/php
	cp -f freeswitch.php $(PREFIX)/lib/php
	cp -f apptest.php $(PREFIX)/scripts
	cp -f test.php $(PREFIX)/scripts
	cp -f php.ini $(PREFIX)/lib
