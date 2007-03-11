LCFLAGS=-fPIC
CFLAGS += -fPIC -I$(PREFIX)/include/python2.4/
PYMOD=freeswitch
LDFLAGS=-lpython2.4 -Xlinker -export-dynamic -L$(PREFIX)/lib/python2.4/config/  -lutil -lstdc++
SWIGCFILE=../../../switch_swig.c
SWIGIFILE=../../../switch_swig.i
CPPCC = g++
OBJS=freeswitch_python.o mod_python_wrap.o

#all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(PYMOD).$(DYNAMIC_LIB_EXTEN)
all:	depends $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(OBJS) 

depends:
	MAKE=$(MAKE) PY_CFLAGS=-fPIC $(BASE)/build/buildlib.sh $(BASE) install Python-2.4.3.tgz --prefix=$(PREFIX) --enable-threads

%.o: %.cpp
	$(CPPCC) $(CFLAGS) -c -o $@ $<

%.o:  %.c
	$(CC) $(LCFLAGS) $(CFLAGS) -c $< -o $@

mod_python.c:
	$(CC) $(LCFLAGS) $(CFLAGS) -c  mod_python.c -o mod_python.o

reswig: 
	# rm -f switch_swig_wrap.c config.m4 CREDITS *${PYMOD}*
	# swig -o switch_swig_wrap.c -l$(SWIGIFILE) -ignoremissing -DMULTIPLICITY -python -module $(PYMOD) $(SWIGCFILE)
	swig -python -shadow -c++ -o mod_python_wrap.cpp mod_python.i
	# patch -p0 -i fix.diff

switch_swig_wrap.o: switch_swig_wrap.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

switch_swig.o: $(SWIGCFILE) Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@


# $(PYMOD).$(DYNAMIC_LIB_EXTEN): $(MODNAME).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o Makefile
#	$(CC) $(SOLINK) -o py_$(PYMOD).$(DYNAMIC_LIB_EXTEN) switch_swig_wrap.o switch_swig.o $(LDFLAGS)

# $(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS) switch_swig_wrap.o switch_swig.o Makefile
#	$(CC) $(LCFLAGS) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o switch_swig_wrap.o switch_swig.o $(OBJS) $(LDFLAGS)

$(MODNAME).$(DYNAMIC_LIB_EXTEN): $(MODNAME).c $(MODNAME).o $(OBJS) Makefile
	$(CC) $(LCFLAGS) $(SOLINK) -o $(MODNAME).$(DYNAMIC_LIB_EXTEN)  $(MODNAME).o $(OBJS) $(LDFLAGS)

clean:
	rm -fr *.$(DYNAMIC_LIB_EXTEN) *.o *~

install:
	$(LTINSTALL) $(MODNAME).$(DYNAMIC_LIB_EXTEN) $(DESTDIR)$(PREFIX)/mod
	cp -f freeswitch.py $(PREFIX)/lib/python2.4/site-packages/

