include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_directory

mod_LTLIBRARIES = mod_directory.la
mod_directory_la_SOURCES  = mod_directory.c
mod_directory_la_CFLAGS   = $(AM_CFLAGS)
mod_directory_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_directory_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
