include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_ladspa

mod_LTLIBRARIES = mod_ladspa.la
mod_ladspa_la_SOURCES  = mod_ladspa.c load.c
mod_ladspa_la_CFLAGS   = $(AM_CFLAGS)
mod_ladspa_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_ladspa_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
