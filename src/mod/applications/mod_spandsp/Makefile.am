include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_spandsp

mod_LTLIBRARIES = mod_spandsp.la
mod_spandsp_la_SOURCES  = mod_spandsp.c udptl.c mod_spandsp_fax.c mod_spandsp_dsp.c mod_spandsp_codecs.c mod_spandsp_modem.c
mod_spandsp_la_CFLAGS   = $(AM_CFLAGS) 
mod_spandsp_la_CPPFLAGS = $(SPANDSP_CFLAGS) -I.
mod_spandsp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la $(SPANDSP_LIBS) -ljpeg -lz -ltiff
mod_spandsp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
