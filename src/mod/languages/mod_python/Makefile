# define these targets in your makefile if you wish
# local_all local_depend local_clean depend_install local_install local_distclean local_extraclean:

# and define these variables to impact your build

LOCAL_CFLAGS=-I$(PREFIX)/include/python2.5/
LOCAL_LDFLAGS=-lpython2.5 -L$(PREFIX)/lib/python2.5/config/  -lutil -lstdc++
LOCAL_OBJS=freeswitch_python.o mod_python_wrap.o
include ../../../../build/modmake.rules

PYMOD=freeswitch

SWIGCFILE=../../../switch_swig.c
SWIGIFILE=../../../switch_swig.i

local_depend:
	MAKE=$(MAKE) $(BASE)/build/buildlib.sh $(BASE) install Python-2.5.1.tgz --prefix=$(PREFIX) --enable-threads CFLAGSFORSHARED="-fPIC"

reswig: 
	swig -python -shadow -c++ -I../../../../src/include -o mod_python_wrap.cpp mod_python.i

switch_swig_wrap.o: switch_swig_wrap.c Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

switch_swig.o: $(SWIGCFILE) Makefile
	$(CC)  -w $(CFLAGS) -c $< -o $@

local_install:
	cp -f freeswitch.py $(DESTDIR)$(PREFIX)/lib/python2.5/site-packages/

