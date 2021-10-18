include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_python3

PYMOD=freeswitch
PYTHON3_SITE_DIR=@PYTHON3_SITE_DIR@

SWIGCFILE=../../../switch_swig.c
SWIGIFILE=../../../switch_swig.i

mod_LTLIBRARIES = mod_python3.la
mod_python3_la_SOURCES  = mod_python3.c freeswitch_python.cpp mod_python_wrap.cpp
mod_python3_la_CFLAGS   = $(SWITCH_AM_CFLAGS)
mod_python3_la_CPPFLAGS = @PYTHON3_CFLAGS@ -I$(switch_srcdir)/libs/libteletone/src/ $(SWITCH_AM_CPPFLAGS)
mod_python3_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_python3_la_LDFLAGS  = -avoid-version -module -no-undefined -shared @PYTHON3_LDFLAGS@

reswig: swigclean mod_python_wrap.cpp

swigclean: clean
	rm -f mod_python_wrap.* freeswitch.py

mod_python_wrap.cpp: mod_python_extra.c hack.diff 
	swig3.0 -python -c++ -I../../../../src/include -oh mod_python_wrap.h -o mod_python_wrap.cpp mod_python.i
	echo "#include \"mod_python_extra.c\"" >> mod_python_wrap.cpp
	patch -s -p0 -i hack.diff

install-data-local:
	mkdir -m755 -p $(DESTDIR)$(PYTHON3_SITE_DIR)
	cp -f freeswitch.py $(DESTDIR)$(PYTHON3_SITE_DIR)

