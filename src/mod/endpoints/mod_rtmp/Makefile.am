include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_rtmp
mod_LTLIBRARIES = mod_rtmp.la
mod_rtmp_la_SOURCES  = mod_rtmp.c rtmp_sig.c rtmp.c rtmp_tcp.c rtmp_video.c
mod_rtmp_la_SOURCES += libamf/src/amf0.c libamf/src/hash.c libamf/src/io.c libamf/src/ptrarray.c libamf/src/types.c 
mod_rtmp_la_CFLAGS   = $(AM_CFLAGS) -Ilibamf/src -I$(abs_srcdir)/libamf/src
mod_rtmp_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_rtmp_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
