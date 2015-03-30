include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_flite

if HAVE_FLITE

mod_LTLIBRARIES = mod_flite.la
mod_flite_la_SOURCES  = mod_flite.c
mod_flite_la_CFLAGS   = $(AM_CFLAGS) $(FLITE_CFLAGS)
mod_flite_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(FLITE_LIBS)
mod_flite_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libflite-dev to build mod_flite)
endif
