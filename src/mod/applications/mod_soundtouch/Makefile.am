include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_soundtouch

if HAVE_SOUNDTOUCH

mod_LTLIBRARIES = mod_soundtouch.la
mod_soundtouch_la_SOURCES  = mod_soundtouch.cpp
mod_soundtouch_la_CFLAGS   = $(AM_CFLAGS) $(SOUNDTOUCH_CFLAGS) -DINTEGER_SAMPLES 
mod_soundtouch_la_CXXFLAGS =  $(SOUNDTOUCH_CFLAGS) -DINTEGER_SAMPLES 
mod_soundtouch_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SOUNDTOUCH_LIBS)
mod_soundtouch_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

else
install: error
all: error
error:
	$(error You must install libsoundtouch-dev to build mod_soundtouch)
endif
