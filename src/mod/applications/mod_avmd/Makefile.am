include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_avmd

mod_LTLIBRARIES = mod_avmd.la
mod_avmd_la_SOURCES  = mod_avmd.c amplitude.c buffer.c desa2.c goertzel.c fast_acosf.c
mod_avmd_la_CFLAGS   = $(AM_CFLAGS)
mod_avmd_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_avmd_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
