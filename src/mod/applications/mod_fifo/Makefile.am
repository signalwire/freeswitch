include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_fifo

mod_LTLIBRARIES = mod_fifo.la
mod_fifo_la_SOURCES  = mod_fifo.c
mod_fifo_la_CFLAGS   = $(AM_CFLAGS)
mod_fifo_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_fifo_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
