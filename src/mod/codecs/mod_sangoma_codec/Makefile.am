include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sangoma_codec

mod_LTLIBRARIES = mod_sangoma_codec.la
mod_sangoma_codec_la_SOURCES  = mod_sangoma_codec.c
mod_sangoma_codec_la_CFLAGS   = $(AM_CFLAGS)
mod_sangoma_codec_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_sangoma_codec_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lsngtc_node
