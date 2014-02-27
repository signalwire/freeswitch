include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_sonar

mod_LTLIBRARIES = mod_sonar.la
mod_sonar_la_SOURCES  = mod_sonar.c
mod_sonar_la_CFLAGS   = $(AM_CFLAGS)
mod_sonar_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_sonar_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
