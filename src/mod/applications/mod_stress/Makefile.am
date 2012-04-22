include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_stress

mod_LTLIBRARIES = mod_stress.la
mod_stress_la_SOURCES  = mod_stress.cpp FFTReal.cpp
mod_stress_la_CFLAGS   = $(AM_CFLAGS)
mod_stress_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_stress_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
