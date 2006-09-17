LCFLAGS=-fPIC -DZTS -DPTHREADS -Xlinker 
CFLAGS += -fPIC -I$(PREFIX)/include/python2.4/
PYMOD=freeswitch
LDFLAGS=-lpython2.4 -Xlinker -L$(PREFIX)/lib/python2.4/config/

all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PYMOD).$(DYNAMIC_LIB_EXTEN)

depends:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install Python-2.4.3.tgz --prefix=$(PREFIX) --enable-threads

%.o:  %.c
	$(CC) $(LCFLAGS) $(CFLAGS) -c $< -o $@

mod_python.c:
	$(CC) $(LCFLAGS) $(CFLAGS) -c  mod_python.c -o mod_python.o

reswig: 
	rm -f switch_swig_wrap.c config.m4 CREDITS *${PYMOD}*
	swig -lswitch_swig.i -ignoremissing -DMULTIPLICITY -python -module $(PYMOD) switch_swig.c
	patch -p0 -i fix.diff

switch_swig_wrap.o: switch_swig_wrap.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

switch_swig.o: switch_swig.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@


$(PYMOD).$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o Makefile
	$(CC) $(SOLINK) -o py_$(PYMOD).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)


$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS) Makefile
	$(CC) $(LCFLAGS) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	#cp -f py_$(PYMOD).$(DYNAMIC_LIB_EXTEN) $(MDIR)
	cp -f $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PREFIX)/mod

