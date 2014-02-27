include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dahdi_codec

mod_LTLIBRARIES = mod_dahdi_codec.la
mod_dahdi_codec_la_SOURCES  = mod_dahdi_codec.c
mod_dahdi_codec_la_CFLAGS   = $(AM_CFLAGS)
mod_dahdi_codec_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_dahdi_codec_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
