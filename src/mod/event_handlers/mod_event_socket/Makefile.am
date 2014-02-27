include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_event_socket

mod_LTLIBRARIES = mod_event_socket.la
mod_event_socket_la_SOURCES  = mod_event_socket.c
mod_event_socket_la_CFLAGS   = $(AM_CFLAGS)
mod_event_socket_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_event_socket_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
