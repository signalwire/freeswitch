include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_gstreamer

if HAVE_GST
mod_LTLIBRARIES = mod_gstreamer.la
mod_gstreamer_la_SOURCES = mod_gstreamer.c gstreamer_api.c
mod_gstreamer_la_CFLAGS = $(AM_CFLAGS)
mod_gstreamer_la_CPPFLAGS = -I. $(GST_CFLAGS) $(AM_CPPFLAGS)
mod_gstreamer_la_LIBADD = $(GST_LIBS) $(switch_builddir)/libfreeswitch.la
mod_gstreamer_la_LDFLAGS = -avoid-version -module -no-undefined -shared
if ISMAC
#mod_gstreamer_la_LDFLAGS += 
endif
else
install: error
all: error
error:
	$(error You must install gstreamer-1.0 and its subprojects to build $(MODNAME))
endif