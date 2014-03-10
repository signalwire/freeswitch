include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_snipe_hunt

mod_LTLIBRARIES = mod_snipe_hunt.la
mod_snipe_hunt_la_SOURCES  = mod_snipe_hunt.c
mod_snipe_hunt_la_CFLAGS   = $(AM_CFLAGS)
mod_snipe_hunt_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_snipe_hunt_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
