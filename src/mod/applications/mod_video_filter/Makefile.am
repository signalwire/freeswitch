include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_video_filter

mod_LTLIBRARIES = mod_video_filter.la
mod_av_la_SOURCES  = mod_video_filter.c
mod_av_la_CFLAGS   = $(AM_CFLAGS)
mod_av_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_av_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz
