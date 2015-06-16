include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_local_stream

mod_LTLIBRARIES = mod_local_stream.la
mod_local_stream_la_SOURCES  = mod_local_stream.c
mod_local_stream_la_CFLAGS   = $(AM_CFLAGS)
mod_local_stream_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_local_stream_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

