include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_valet_parking

mod_LTLIBRARIES = mod_valet_parking.la
mod_valet_parking_la_SOURCES  = mod_valet_parking.c
mod_valet_parking_la_CFLAGS   = $(AM_CFLAGS)
mod_valet_parking_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_valet_parking_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
