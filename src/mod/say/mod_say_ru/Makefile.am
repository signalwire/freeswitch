include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_say_ru

mod_LTLIBRARIES = mod_say_ru.la
mod_say_ru_la_SOURCES  = mod_say_ru.c
mod_say_ru_la_CFLAGS   = $(AM_CFLAGS)
mod_say_ru_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_say_ru_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
