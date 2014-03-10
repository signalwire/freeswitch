include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_nibblebill

mod_LTLIBRARIES = mod_nibblebill.la
mod_nibblebill_la_SOURCES  = mod_nibblebill.c
mod_nibblebill_la_CFLAGS   = $(AM_CFLAGS)
mod_nibblebill_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_nibblebill_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
