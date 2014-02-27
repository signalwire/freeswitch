include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_mp4

mod_LTLIBRARIES = mod_mp4.la
mod_mp4_la_SOURCES  = mod_mp4.cpp mp4_helper.cpp
mod_mp4_la_CFLAGS   = $(AM_CFLAGS)
mod_mp4_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_mp4_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lmp4v2
