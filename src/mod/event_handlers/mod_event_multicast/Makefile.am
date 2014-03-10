include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_event_multicast

mod_LTLIBRARIES = mod_event_multicast.la
mod_event_multicast_la_SOURCES  = mod_event_multicast.c
mod_event_multicast_la_CFLAGS   = $(AM_CFLAGS) $(OPENSSL_CFLAGS)
mod_event_multicast_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_event_multicast_la_LDFLAGS  = -avoid-version -module -no-undefined -shared $(OPENSSL_LIBS)
