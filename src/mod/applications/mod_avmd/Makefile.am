include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_avmd

mod_LTLIBRARIES = mod_avmd.la
mod_avmd_la_SOURCES  = mod_avmd.c avmd_buffer.c avmd_desa2_tweaked.c avmd_fast_acosf.c
mod_avmd_la_CFLAGS   = $(AM_CFLAGS) $(AM_MOD_AVMD_CXXFLAGS)
mod_avmd_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_avmd_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
