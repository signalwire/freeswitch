include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_hr

mod_LTLIBRARIES = mod_say_hr.la
mod_say_hr_la_SOURCES  = mod_say_hr.c
mod_say_hr_la_CFLAGS   = $(AM_CFLAGS)
mod_say_hr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_hr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
