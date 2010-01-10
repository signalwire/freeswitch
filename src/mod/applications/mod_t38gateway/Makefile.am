include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_t38gateway

mod_LTLIBRARIES = mod_t38gateway.la
mod_t38gateway_la_SOURCES  = mod_t38gateway.c udptl.c
mod_t38gateway_la_CFLAGS   = $(AM_CFLAGS)
mod_t38gateway_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_t38gateway_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
