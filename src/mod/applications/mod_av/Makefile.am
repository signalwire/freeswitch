include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_av

mod_LTLIBRARIES = mod_av.la
mod_av_la_SOURCES  = mod_av.c
mod_av_la_CFLAGS   = $(AM_CFLAGS)
mod_av_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_av_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lavformat -lavcodec -lavutil -lswresample -lx264

