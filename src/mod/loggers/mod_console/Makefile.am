include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_console

mod_LTLIBRARIES = mod_console.la
mod_console_la_SOURCES  = mod_console.c
mod_console_la_CFLAGS   = $(AM_CFLAGS)
mod_console_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_console_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
