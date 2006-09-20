LCFLAGS=-fPIC -DZTS -DPTHREADS
CFLAGS += -fPIC -I../../../../libs/ruby-1.8.5/
RBMOD=freeswitch
LDFLAGS=-lruby-static -L$(PREFIX)/lib/ 
SWIGCFILE=../../../switch_swig.c
SWIGIFILE=../../../switch_swig.i

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(RBMOD).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) MOD_CFLAGS=-fPIC $(BASE)/build/buildlib.sh $(BASE) install ruby-1.8.5.tar.gz --prefix=$(PREFIX)
	cp -f config.h ../../../../libs/ruby-1.8.5/

%.o:  %.c
	$(CC) $(LCFLAGS) $(CFLAGS) -c $< -o $@

mod_ruby.c:
	$(CC) $(LCFLAGS) $(CFLAGS) -c mod_ruby.c -o mod_ruby.o

reswig: 
	rm -f switch_swig_wrap.c config.m4 CREDITS *${RBMOD}*
	swig -o switch_swig_wrap.c -l$(SWIGIFILE) -ignoremissing -DMULTIPLICITY -ruby -module $(RBMOD) $(SWIGCFILE)

switch_swig_wrap.o: switch_swig_wrap.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

switch_swig.o: $(SWIGCFILE) Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@


$(RBMOD).$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o Makefile
	$(CC) $(SOLINK) -o rb_$(RBMOD).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS) Makefile
	$(CC) $(LCFLAGS) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	#cp -f rb_$(RBMOD).$(DYNAMIC_LIB_EXTEN) $(MDIR)
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod
