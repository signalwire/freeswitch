include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_video_filter

mod_LTLIBRARIES = mod_video_filter.la
mod_video_filter_la_SOURCES  = mod_video_filter.c
mod_video_filter_la_CFLAGS   = $(AM_CFLAGS)
mod_video_filter_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_video_filter_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm -lz
