include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_posix_timer

mod_LTLIBRARIES = mod_posix_timer.la
mod_posix_timer_la_SOURCES  = mod_posix_timer.c
mod_posix_timer_la_CFLAGS   = $(AM_CFLAGS)
mod_posix_timer_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_posix_timer_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
