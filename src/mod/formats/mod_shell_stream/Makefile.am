include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_shell_stream

mod_LTLIBRARIES = mod_shell_stream.la
mod_shell_stream_la_SOURCES  = mod_shell_stream.c
mod_shell_stream_la_CFLAGS   = $(AM_CFLAGS)
mod_shell_stream_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_shell_stream_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
