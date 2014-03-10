include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_native_file

mod_LTLIBRARIES = mod_native_file.la
mod_native_file_la_SOURCES  = mod_native_file.c
mod_native_file_la_CFLAGS   = $(AM_CFLAGS)
mod_native_file_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_native_file_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
