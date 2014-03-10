include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_timerfd

mod_LTLIBRARIES = mod_timerfd.la
mod_timerfd_la_SOURCES  = mod_timerfd.c
mod_timerfd_la_CFLAGS   = $(AM_CFLAGS)
mod_timerfd_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_timerfd_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
