include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_dialplan_directory

mod_LTLIBRARIES = mod_dialplan_directory.la
mod_dialplan_directory_la_SOURCES  = mod_dialplan_directory.c
mod_dialplan_directory_la_CFLAGS   = $(AM_CFLAGS)
mod_dialplan_directory_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_dialplan_directory_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
