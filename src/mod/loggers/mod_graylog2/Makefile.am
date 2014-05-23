include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_graylog2

mod_LTLIBRARIES = mod_graylog2.la
mod_graylog2_la_SOURCES  = mod_graylog2.c
mod_graylog2_la_CFLAGS   = $(AM_CFLAGS)
mod_graylog2_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_graylog2_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
