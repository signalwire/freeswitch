include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_commands

mod_LTLIBRARIES = mod_commands.la
mod_commands_la_SOURCES  = mod_commands.c
mod_commands_la_CFLAGS   = $(AM_CFLAGS)
mod_commands_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_commands_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
