include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_logfile

mod_LTLIBRARIES = mod_logfile.la
mod_logfile_la_SOURCES  = mod_logfile.c
mod_logfile_la_CFLAGS   = $(AM_CFLAGS)
mod_logfile_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_logfile_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
