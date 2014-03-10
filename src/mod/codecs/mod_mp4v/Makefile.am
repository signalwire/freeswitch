include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mp4v

mod_LTLIBRARIES = mod_mp4v.la
mod_mp4v_la_SOURCES  = mod_mp4v.c
mod_mp4v_la_CFLAGS   = $(AM_CFLAGS)
mod_mp4v_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_mp4v_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
