include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_skel_codec

mod_LTLIBRARIES = mod_skel_codec.la
mod_skel_codec_la_SOURCES  = mod_skel_codec.c
mod_skel_codec_la_CFLAGS   = $(AM_CFLAGS)
mod_skel_codec_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_skel_codec_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
